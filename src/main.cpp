#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;
            write_message(message);
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};


class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {

    /** 1 SENSOR PARA SERVIDOR
     * Salvar as leituras dos sensores num arquivo unico por sensor 
     * (usar a pasta "registros e por uns txt lá")
     * 
     * Formatar os dados registrados LOG|SENSOR_ID|DATA_HORA|LEITURA\r\n
     * Ter uma função para criar essa formatação
     * ex LOG|SENSOR_001|2023-05-11T15:30:00|78.5\r\n
    */

   /** 2 CLIENTE PARA O SERVIDOR
    * Formatar as mensagens para GET|SENSOR_ID|NUMERO_DE_REGISTROS\r\n
    * ex GET|SENSOR_001|10\r\n
    * ou seja, vamos receber isso do cliente e devolver os registros q salvamos em 1
   */

    /** 3 SERVIDOR PARA CLIENTE
     * resposta padrão se existir o sensor:
     * NUM_REGISTROS;DATA_HORA|LEITURA;...;DATA_HORA|LEITURA\r\n
     * 
     * erro que será retornado se o sensor não existir:
     * ERROR|INVALID_SENSOR_ID\r\n
    */
   if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;

}

// (cliente) python3 sensor_emulator.py --ip 127.0.0.1 --port 8080 --sensor_id my_sensor --frequency 500

/** main (tem q ter compilado)
 * cd ..
 * cd build
 * ./das 8080
 * 
*/ 

