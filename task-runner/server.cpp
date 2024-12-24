#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <map>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct Session {
    tcp::socket socket;
    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    
    explicit Session(tcp::socket&& s): socket(std::move(s)) {}
};

class HTTPServer
{

public:

    short unsigned int port;

    HTTPServer(short unsigned int port = 8080) : port(port) {}

    void run()
    {
        run_server(this->port);
    }

    void subscribe_endpoint_method(
        http::verb method, 
        std::string endpoint, 
        std::function<void(const http::request<http::string_body>&, http::status*, std::string*)> callback) 
    {
        endpoint_handlers[std::make_pair(method, endpoint)] = callback;
    }

private:

    std::map<std::pair<http::verb, std::string>, 
             std::function<void(const http::request<http::string_body>&, 
                              http::status*, 
                              std::string*)>> endpoint_handlers;

    void run_server(unsigned short port) {
        asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), this->port}};
        
        accept_query(acceptor);
        ioc.run();
    }

    void accept_query(tcp::acceptor& acceptor) {
        auto socket = std::make_shared<tcp::socket>(acceptor.get_executor());
        
        acceptor.async_accept(*socket,
            [this, &acceptor, socket](beast::error_code error) {
                if (!error) {
                    auto session = std::make_shared<Session>(std::move(*socket));
                    http::async_read(session->socket, session->buffer, session->request,
                        [this, session](beast::error_code ec, std::size_t) {
                            if (!ec) {

                                http::status status;
                                std::string body;
                                process_request(session, &status, &body);
                                send_response(session, status, body);
                            }
                    });
                }
                accept_query(acceptor);
            });
    }

    void process_request(std::shared_ptr<Session> session, http::status* status, std::string* body) {
        auto key = std::make_pair(session->request.method(), std::string(session->request.target()));
        
        auto handler = endpoint_handlers.find(key);
        if (handler != endpoint_handlers.end()) {
            handler->second(session->request, status, body);
        } else {
            *status = http::status::not_found;
            *body = "Эндпоинт не найден";
        }
    }

    void send_response(std::shared_ptr<Session> session, 
                  http::status status, 
                  const std::string& body) {
        auto response = std::make_shared<http::response<http::string_body>>(
            status, session->request.version());
        response->set(http::field::content_type, "application/json");
        response->body() = body;
        response->prepare_payload();

        http::async_write(session->socket, *response,
            [session, response](beast::error_code ec, std::size_t) {
                session->socket.shutdown(tcp::socket::shutdown_send, ec);
            });
    }

};