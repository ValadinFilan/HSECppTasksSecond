#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <linux/input.h>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include "opencv4/opencv2/highgui/highgui.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

beast::tcp_stream create_connection(const std::string& host, const std::string& port) {
    asio::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);
    
    auto const results = resolver.resolve(host, port);
    stream.connect(results);
    return stream;
}

void send_post(const std::string& host, const std::string& port, const std::string& target, const std::string& json_body) {
    auto stream = create_connection(host, port);
    
    http::request<http::string_body> request{http::verb::post, target, 11};
    request.set(http::field::host, host);
    request.set(http::field::content_type, "application/json");
    request.body() = json_body;
    request.prepare_payload();

    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> response;
    http::read(stream, buffer, response);

    beast::error_code error;
    stream.socket().shutdown(tcp::socket::shutdown_both, error);
}

void download_cats() {
    for(int i = 0; i < 12; i++) {
        auto stream = create_connection("algisothal.ru", "8888");

        http::request<http::string_body> request{http::verb::get, "/cat", 11};
        http::write(stream, request);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> response;
        http::read(stream, buffer, response);

        std::string filename = "./images/cat_" + std::to_string(i) + ".jpg";
        std::ofstream out(filename, std::ios::binary);
        out << beast::buffers_to_string(response.body().data());
        out.close();

        beast::error_code error;
        stream.socket().shutdown(tcp::socket::shutdown_both, error);

        std::cout << "Downloaded image " << (i + 1) << std::endl;
    }
}

void create_collage() {
    cv::Mat collage(2400, 2400, CV_8UC3);
        
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 3; col++) {
            int index = row * 3 + col;
            std::string filename = "./images/cat_" + std::to_string(index) + ".jpg";
                
            cv::Mat image = cv::imread(filename);                
            cv::resize(image, image, cv::Size(800, 600));
            
            cv::Mat roi = collage(cv::Rect(col * 800, row * 600, 800, 600));
            image.copyTo(roi);
        }
    }
        
    cv::imwrite("./images/collage.jpg", collage);
}

void upload_collage() {
    auto stream = create_connection("algisothal.ru", "8889");

    std::ifstream file("./images/collage.jpg", std::ios::binary);
    
    std::stringstream file_content;
    file_content << file.rdbuf();
    std::string content = file_content.str();

    std::string boundary = "-------------------------boundary123";
    std::string body = "--" + boundary + "\r\n"
                      "Content-Disposition: form-data; name=\"file\"; filename=\"collage.jpg\"\r\n"
                      "Content-Type: image/jpeg\r\n\r\n" +
                      content + "\r\n--" + boundary + "--\r\n";

    http::request<http::string_body> request_collage{http::verb::post, "/cat", 11};
    request_collage.set(http::field::content_type, "multipart/form-data; boundary=" + boundary);
    request_collage.set(http::field::content_length, std::to_string(body.length()));
    request_collage.body() = body;

    http::write(stream.socket(), request_collage);

    beast::flat_buffer buffer;
    http::response<http::string_body> result;
    http::read(stream.socket(), buffer, result);

    std::cout << result << std::endl;

    stream.socket().shutdown(tcp::socket::shutdown_both);
}

int main() {
    download_cats();    
    create_collage();
    upload_collage();
}