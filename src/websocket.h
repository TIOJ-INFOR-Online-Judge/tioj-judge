#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include <variant>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>

class WsClient {
 public:
  using NoTLSClient = websocketpp::client<websocketpp::config::asio_client>;
  using TLSClient = websocketpp::client<websocketpp::config::asio_tls_client>;
  using Thread = std::shared_ptr<websocketpp::lib::thread>;
  using Handle = websocketpp::connection_hdl;
  using TLSContext = boost::asio::ssl::context;
  using TLSContextPtr = std::shared_ptr<TLSContext>;
 private:
  std::variant<NoTLSClient, TLSClient> client_;
  Thread thr_;

  std::string url_;
  Handle hdl_;
  bool connected_, close_issued_;

  template <class Client> void Init_(Client& c) {
    using namespace websocketpp::lib;
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.clear_error_channels(websocketpp::log::elevel::all);
    c.init_asio();
    c.start_perpetual();
    if constexpr (std::is_same<TLSClient, Client>::value) {
      c.set_tls_init_handler(bind(&WsClient::OnTLSInit_, this, placeholders::_1));
    }
    thr_ = std::make_shared<thread>(&Client::run, &c);
  }

  template <class Client> void Destroy_(Client& c) {
    c.stop_perpetual();
    if (connected_) {
      c.close(hdl_, websocketpp::close::status::going_away, "");
    }
  }

  TLSContextPtr OnTLSInit_(Handle) {
    using namespace boost::asio::ssl;
    TLSContextPtr ctx = std::make_shared<context>(context::sslv23);
    ctx->set_options(context::default_workarounds | context::no_sslv2 | context::no_sslv3 | context::single_dh_use);
    return ctx;
  }
  void OnOpen_(Handle hdl) {
    connected_ = true;
    hdl_ = hdl;
    OnOpen();
  }
  void OnFail_(Handle) {
    OnFail();
  }
  void OnClose_(Handle) {
    hdl_ = Handle();
    connected_ = false;
    close_issued_ = false;
    OnClose();
  }
  template <class Client> void OnMessage_(Handle, typename Client::message_ptr msg) {
    OnMessage(msg->get_payload());
  }

  template <class Client> bool Connect_(Client& c) {
    using namespace websocketpp::lib;
    error_code ec;
    typename Client::connection_ptr conn = c.get_connection(url_, ec);
    if (ec) return false;
    conn->set_open_handler(bind(&WsClient::OnOpen_, this, placeholders::_1));
    conn->set_fail_handler(bind(&WsClient::OnFail_, this, placeholders::_1));
    conn->set_close_handler(bind(&WsClient::OnClose_, this, placeholders::_1));
    conn->set_message_handler(bind(&WsClient::OnMessage_<Client>, this,
          placeholders::_1, placeholders::_2));
    c.connect(conn);
    return true;
  }

  template <class Client> bool Send_(Client& c, const std::string& str) {
    websocketpp::lib::error_code ec;
    c.send(hdl_, str, websocketpp::frame::opcode::text, ec);
    return !ec;
  }

  template <class Client> bool Close_(Client& c) {
    websocketpp::lib::error_code ec;
    c.close(hdl_, websocketpp::close::status::going_away, "");
    if (!ec) close_issued_ = true;
    return !ec;
  }

 public:
  WsClient(const std::string& url);
  ~WsClient();

  bool IsTLS() const { return client_.index() == 1; }
  bool IsConnected() const { return connected_; }
  bool CanSend() const { return connected_ && !close_issued_; }

  // Note: if you want to call connection operations such as Connect() in these functions,
  //   create a thread to do it; Send() can be called directly
  virtual void OnOpen() {}
  virtual void OnFail() {}
  virtual void OnClose() {}
  virtual void OnMessage(const std::string& str) {}

  bool Connect();
  bool Send(const std::string& str);
  bool Close();
};

#endif // WEBSOCKET_H_
