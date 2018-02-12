/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_SOCKET_H
#define ASIO_HTTP_SOCKET_H

#include <boost/asio.hpp>

namespace asio_http
{
namespace internal
{
class transport_layer
{
public:
  transport_layer(transport_layer* upper)
      : upper_layer(upper)
  {
  }
  virtual ~transport_layer() {}
  virtual void connect(boost::asio::ip::tcp::resolver::iterator it) {}
  virtual void on_connected(const boost::system::error_code& ec) {}
  virtual void read() {}
  virtual void on_read(const uint8_t* data, std::size_t size, boost::system::error_code ec) {}
  virtual void write(std::vector<uint8_t> data) {}
  virtual void on_write(const boost::system::error_code& ec) {}
  virtual void close() {}
  virtual bool is_open() { return false; }
  virtual void set_upper(transport_layer* upper) { upper_layer = upper; }

protected:
  transport_layer* upper_layer;
  void             call_on_read(const uint8_t* data, std::size_t size, boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_read(data, size, ec);
    }
  }
  void call_on_connected(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_connected(ec);
    }
  }
  void call_on_write(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_write(ec);
    }
  }
};

class tcp_socket
    : public transport_layer
    , public std::enable_shared_from_this<tcp_socket>
{
public:
  tcp_socket(transport_layer* upper, boost::asio::io_context& context)
      : transport_layer(upper)
      , m_read_buffer(1024)
      , m_socket(context)
  {
  }

  virtual bool is_open() override { return m_socket.is_open(); }

  virtual void connect(boost::asio::ip::tcp::resolver::iterator it) override
  {
    boost::asio::ip::tcp::endpoint ep = *it;
    m_socket.async_connect(ep, [ ptr = shared_from_this(), it ](auto&& ec) { ptr->connect_handler(ec, it); });
  }

  virtual void write(std::vector<uint8_t> data) override
  {
    std::swap(m_write_buffer, data);
    boost::asio::async_write(m_socket,
                             boost::asio::const_buffer(m_write_buffer.data(), m_write_buffer.size()),
                             [ptr = shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_handler(ec, bytes); });
  }

  virtual void read() override
  {
    m_socket.async_read_some(boost::asio::buffer(m_read_buffer), [ptr = shared_from_this()](auto&& ec, auto&& bytes) {
      ptr->read_handler(ec, bytes);
    });
  }

  virtual void close() override { m_socket.close(); }

private:
  boost::asio::ip::tcp::socket m_socket;
  std::vector<uint8_t>         m_write_buffer;
  std::vector<uint8_t>         m_read_buffer;

  void connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (!ec)
    {
      call_on_connected(ec);
    }
    else if (it != boost::asio::ip::tcp::resolver::iterator())
    {
      boost::asio::ip::tcp::endpoint ep = *it;
      m_socket.async_connect(ep, [ ptr = shared_from_this(), it ](auto&& ec) { ptr->connect_handler(ec, it); });
    }
    else
    {
      call_on_connected(ec);
    }
  }

  void write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred) { call_on_write(ec); }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
  {
    call_on_read(m_read_buffer.data(), bytes_transferred, ec);
  }
};
}
}

#endif
