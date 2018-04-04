/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2018 XMRig       <support@xmrig.com>
 * Copyright 2017-     BenDr0id    <ben@graef.in>
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


#include "interfaces/IStrategyListener.h"
#include "net/Client.h"
#include "net/Job.h"
#include "net/strategies/DonateStrategy.h"
#include "Options.h"


extern "C"
{
#include "crypto/c_keccak.h"
}

static inline int random(int min, int max) {
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

DonateStrategy::DonateStrategy(const char *agent, IStrategyListener *listener) :
    m_active(false),
    m_donateTime(Options::i()->donateLevel() * 60 * 1000),
    m_idleTime((100 - Options::i()->donateLevel()) * 60 * 1000),
    m_listener(listener)
{
    uint8_t hash[200];
    char userId[65] = { 0 };
    const char *user = Options::i()->pools().front()->user();

    keccak(reinterpret_cast<const uint8_t *>(user), static_cast<int>(strlen(user)), hash, sizeof(hash));
    Job::toHex(hash, 32, userId);

    Url *url;

#ifndef XMRIG_NO_TLS
    if (Options::i()->forcePowVersion() == Options::POW_V1) {
        url = new Url("donate.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 8080 : 8081, userId, nullptr, true, false, true);
    } else if (Options::i()->forcePowVersion() == Options::POW_V2) {
        url = new Url("donate2.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 995 : 993, userId, nullptr, true, false, true);
    } else {
        url = new Url("donate2.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 8081 : 443, userId, nullptr, true, false, true);
    }
#else
    if (Options::i()->forcePowVersion() == Options::POW_V1) {
        url = new Url("donate.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 80 : 443, userId, nullptr, false, false, true);
    } else if (Options::i()->forcePowVersion() == Options::POW_V2) {
        url = new Url("donate.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 995 : 993, userId, nullptr, false, false, true);
    } else {
        url = new Url("donate2.graef.in", Options::i()->algo() == Options::ALGO_CRYPTONIGHT_LITE ? 8080 : 80, userId, nullptr, false, false, true);
    }
#endif

    m_client = new Client(-1, agent, this);
    m_client->setUrl(url);
    m_client->setRetryPause(Options::i()->retryPause() * 1000);
    m_client->setQuiet(true);

    delete url;

    m_timer.data = this;
    uv_timer_init(uv_default_loop(), &m_timer);

    idle(random(1800, 3600) * 1000);
}


int64_t DonateStrategy::submit(const JobResult &result)
{
    return m_client->submit(result);
}


void DonateStrategy::connect()
{
    m_client->connect();
}


void DonateStrategy::stop()
{
    uv_timer_stop(&m_timer);
    m_client->disconnect();
}


void DonateStrategy::tick(uint64_t now)
{
    m_client->tick(now);
}


void DonateStrategy::onClose(Client *client, int failures)
{
}


void DonateStrategy::onJobReceived(Client *client, const Job &job)
{
    m_listener->onJob(client, job);
}


void DonateStrategy::onLoginSuccess(Client *client)
{
    if (!isActive()) {
        uv_timer_start(&m_timer, DonateStrategy::onTimer, m_donateTime, 0);
    }

    m_active = true;
    m_listener->onActive(client);
}


void DonateStrategy::onResultAccepted(Client *client, const SubmitResult &result, const char *error)
{
    m_listener->onResultAccepted(client, result, error);
}


void DonateStrategy::idle(uint64_t timeout)
{
    uv_timer_start(&m_timer, DonateStrategy::onTimer, timeout, 0);
}


void DonateStrategy::suspend()
{
    m_client->disconnect();

    m_active = false;
    m_listener->onPause(this);

    idle(m_idleTime);
}


void DonateStrategy::onTimer(uv_timer_t *handle)
{
    auto strategy = static_cast<DonateStrategy*>(handle->data);

    if (!strategy->isActive()) {
        return strategy->connect();
    }

    strategy->suspend();
}
