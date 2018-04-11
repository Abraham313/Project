/* XMRigCC
 * Copyright 2018      Sebastian Stolzenberg <https://github.com/sebastianstolzenberg>
 * Copyright 2018-     BenDr0id <ben@graef.in>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BOOSTCONNECTION_H__
#define __BOOSTCONNECTION_H__

#include "net/Connection.h"
#include "log/Log.h"

template <class SOCKET>
class BoostConnection : public Connection
{
public:
    BoostConnection(const ConnectionListener::Ptr& listener,
                    const std::string& server, uint16_t port)
            : Connection(listener)
            , socket_(ioService_)
    {
        LOG_DEBUG("[%s:%d] Connecting", server.c_str(), port);

        boost::asio::ip::tcp::resolver resolver(ioService_);
        boost::asio::ip::tcp::resolver::query query(server, std::to_string(port));
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

        socket_.connect(iterator);

        triggerRead();

        std::thread([this]() { ioService_.run(); }).detach();

        LOG_DEBUG("[%s:%d] Connected", server.c_str(), port);
    }

    ~BoostConnection()
    {
        LOG_DEBUG("[%s:%d] Shutdown", getConnectedIp().c_str(), getConnectedPort());

        socket_.get().lowest_layer().close();

        ioService_.stop();
    }

    bool isConnected() const override
    {
        return socket_.get().lowest_layer().is_open();
    }

    std::string getConnectedIp() const override
    {
        return isConnected() ? socket_.get().lowest_layer().remote_endpoint().address().to_string() : "";
    }

    uint16_t getConnectedPort() const override
    {
        return isConnected() ? socket_.get().lowest_layer().remote_endpoint().port() : 0;
    }

    bool send(const char* data, std::size_t size)
    {
        LOG_DEBUG("[%s:%d] Sending: %.*s", getConnectedIp().c_str(), getConnectedPort(), size, data);

        boost::system::error_code error;
        boost::asio::write(socket_.get(), boost::asio::buffer(data, size), error);
        if (error)
        {
            LOG_DEBUG_ERR("[%s:%d] Sending failed: %s", getConnectedIp().c_str(), getConnectedPort(), error.message().c_str());
            notifyError(error.message());
        }
        return !error;
    }

    void triggerRead()
    {
        boost::asio::async_read(socket_.get(),
                                boost::asio::buffer(receiveBuffer_, sizeof(receiveBuffer_)),
                                boost::asio::transfer_at_least(1),
                                boost::bind(&BoostConnection::handleRead, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void handleRead(const boost::system::error_code& error,
                    size_t bytes_transferred)
    {
        if (!error)
        {
            LOG_DEBUG("[%s:%d] Read: %.*s", getConnectedIp().c_str(), getConnectedPort(), bytes_transferred, receiveBuffer_);
            notifyRead(receiveBuffer_, bytes_transferred);
            triggerRead();
        }
        else
        {
            LOG_DEBUG_ERR("[%s:%d] Read failed: %s", getConnectedIp().c_str(), getConnectedPort(), error.message().c_str());
            notifyError(error.message());
        }
    }

private:
    boost::asio::io_service ioService_;
    SOCKET socket_;
    char receiveBuffer_[2048];
};

#endif /* __BOOSTCONNECTION_H__ */