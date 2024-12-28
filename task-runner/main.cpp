#include "server.cpp"

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>

std::pair<std::string, std::string> execute_command(const std::string& command, const std::vector<std::string>& args) {
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(command.c_str()));
    
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    int stdout_pipe[2];
    int stderr_pipe[2];
    pipe(stdout_pipe);
    pipe(stderr_pipe);

    pid_t pid = fork();
    
    if (pid == 0) { 
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        
        execvp(command.c_str(), c_args.data());
        
        std::cerr << "Ошибка выполнения команды: " << command << std::endl;
        exit(1);
    } else if (pid > 0) {
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::string stdout_output, stderr_output;
        char buffer[4096];
        ssize_t bytes_read;

        while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            stdout_output.append(buffer, bytes_read);
        }
        while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            stderr_output.append(buffer, bytes_read);
        }

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        return {stdout_output, stderr_output};
    } else {
        return {"", "Ошибка при создании процесса"};
    }
}


int main() {
    HTTPServer server;
    server.subscribe_endpoint_method(http::verb::post, "/run", 
        [](const auto& request, http::status* response_status, std::string* response_body) {
            
            boost::property_tree::ptree pt;
            std::stringstream ss(request.body());
            boost::property_tree::read_json(ss, pt);

            std::string command = pt.get_child("task").begin()->second.get_value<std::string>();
            std::vector<std::string> args;
            
            auto task_array = pt.get_child("task");
            auto it = task_array.begin();
            ++it;

            for (; it != task_array.end(); ++it) {
                args.push_back(it->second.get_value<std::string>());
            }

            auto [stdout_output, stderr_output] = execute_command(command, args);    
            *response_body = "{ \"stdout\": \"" + stdout_output + "\", \"stderr\": \"" + stderr_output + "\" }";
            *response_status = http::status::ok;
            
    });
    server.run();
}