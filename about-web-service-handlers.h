struct FileRequestHandler final : public Poco::Net::HTTPRequestHandler {
  virtual void
  handleRequest(Poco::Net::HTTPServerRequest &request,
                Poco::Net::HTTPServerResponse &response) override final {
    std::cerr << "Handling file request" << std::endl;
    request.write(std::cerr);
    std::cerr << "Request URI (raw): " << request.getURI() << std::endl;
    std::cerr << "Number of threads " << Poco::ThreadPool::defaultPool().used()
              << std::endl;

    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN,
                                Poco::Net::HTTPResponse::getReasonForStatus(
                                    Poco::Net::HTTPResponse::HTTP_FORBIDDEN));
    response.setContentType("text/plain");

    if (request.getMethod() == "GET" &&
        std::regex_match(
            request.getURI(),
            std::regex{
                R"(/?(?:(index.html?)?|data/.+?\.te?xt|scripts/.+?\.js|styles/.+?\.css|fonts/.+?\.(ttf|otf|woff)|images/.+?\.(png|svg|webp|ico|gif|jpe?g|bmp)))"})) {
      auto uri = fs::absolute(fs::current_path() / request.getURI());
      try {
        response.setStatusAndReason(
            Poco::Net::HTTPServerResponse::HTTP_OK,
            Poco::Net::HTTPServerResponse::getReasonForStatus(
                Poco::Net::HTTPServerResponse::HTTP_OK));
        response.sendFile(uri, mime_types[uri.extension()]);
      } catch (Poco::FileNotFoundException &e) {
        response.setStatusAndReason(
            Poco::Net::HTTPServerResponse::HTTP_NOT_FOUND, e.name());
        response.sendBuffer(e.message().c_str(), e.message().length());
      } catch (Poco::OpenFileException &e) {
        response.setStatusAndReason(
            Poco::Net::HTTPServerResponse::HTTP_UNAUTHORIZED, e.name());
        response.sendBuffer(e.message().c_str(), e.message().length());
      } catch (std::runtime_error &e) {
        response.setStatusAndReason(
            Poco::Net::HTTPServerResponse::HTTP_UNAUTHORIZED, "Runtime error");
        response.sendBuffer(e.what(), std::strlen(e.what()));
      }
    } else if (request.getMethod() == "POST" &&
               std::regex_match(request.getURI(), std::regex{"/?shutdown"})) {
      response.setStatusAndReason(
          Poco::Net::HTTPServerResponse::HTTP_NO_CONTENT,
          "App requested shutdown");
      response.send();

      std::lock_guard<std::mutex>{shutdown_m};
      shutdown_request = true;
      shutdown_cv.notify_all();
    } else
      response.send();

    std::cerr << "Responding" << std::endl;
    response.write(std::cerr);
  }
};

struct ServiceRequestHandler final : public Poco::Net::HTTPRequestHandler {
  virtual void
  handleRequest(Poco::Net::HTTPServerRequest &request,
                Poco::Net::HTTPServerResponse &response) override final {
    Poco::Net::WebSocket socket{request, response};
    socket.setReceiveTimeout(Poco::Timespan{15, 0});
    std::cerr << "Handling web socket" << std::endl;
    request.write(std::cerr);
    response.write(std::cerr);
    std::cerr << "Request URI (raw): " << request.getURI() << std::endl;
    std::cerr << "Number of threads " << Poco::ThreadPool::defaultPool().used()
              << std::endl;

    try {

      int flags;
      std::cerr << "Waiting for frame" << std::endl;
      for (auto read = receive_handle_control_frame(socket, small_buffer.data(),
                                                    small_buffer.size(), flags);
           read > 0;
           read = receive_handle_control_frame(socket, small_buffer.data(),
                                               small_buffer.size(), flags)) {
        if (!(flags & Poco::Net::WebSocket::FRAME_TEXT))
          throw std::string{"Expecting a complete text frame"};
        std::regex call_rx{R"(^Calling (\d+)\(\)\.\.\.$)"};
        if (!std::regex_match(begin(small_buffer), begin(small_buffer) + read,
                              call_rx))
          throw std::string{"Expecting a call eyecatcher"};
        std::cerr << "Call eyecatcher: \""
                  << std::string{begin(small_buffer),
                                 begin(small_buffer) + read}
                  << '"' << std::endl;
        std::string callee;
        std::regex_replace(std::back_inserter(callee), begin(small_buffer),
                           begin(small_buffer) + read, call_rx, "$1");
        std::cerr << "Callee: " << callee << std::endl;
        auto func =
            get_rpc_handler(funcs, reinterpret_cast<rpc>(std::stoull(callee)));
        func(socket);
        std::cerr << "Waiting for frame" << std::endl;
      }

    } catch (const std::string &error) {
      std::cerr << error << std::endl;
      socket.sendFrame(nullptr, 0, Poco::Net::WebSocket::FRAME_OP_CLOSE);
    } catch (...) {
    }
    socket.close();
  }
};

struct AboutWebRequestHandlerFactory final
    : public Poco::Net::HTTPRequestHandlerFactory {
  virtual Poco::Net::HTTPRequestHandler *createRequestHandler(
      const Poco::Net::HTTPServerRequest &request) override final {
    std::cerr << "Finding handler" << std::endl;
    std::cerr << "Number of threads: " << Poco::ThreadPool::defaultPool().used()
              << std::endl;
    request.write(std::cerr);
    if (request.hasToken("connection", "upgrade") ||
        request.hasToken("upgrade", "websocket"))
      return new ServiceRequestHandler;
    return new FileRequestHandler;
  }
};
