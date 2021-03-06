#include <nc_hashkit.h>

static rstatus_t random_update(NcServerPool *pool)
{
    #define RANDOM_CONTINUUM_ADDITION   10  /* # extra slots to build into continuum */
    #define RANDOM_POINTS_PER_SERVER    1

    int64_t now = NcUtil::ncUsecNow();

    if (now < 0)
    {
        LOG_WARN("now < 0");
        return NC_ERROR;
    }

    uint32_t nserver = pool->server.size();
    uint32_t nlive_server = 0;
    pool->next_rebuild = 0LL;

    for (int i = 0; i < nserver; i++)
    {
        NcServer *server = (pool->server)[i];
        if (pool->auto_eject_hosts) 
        {
            if (server->getNextRetry() <= now) 
            {
                server->setNextRetry(0LL);
                nlive_server++;
            } 
            else if (pool->next_rebuild == 0LL || server->getNextRetry() < pool->next_rebuild) 
            {
                pool->next_rebuild = server->getNextRetry();
            }
            else
            {
                // pass
            }
        } 
        else 
        {
            nlive_server++;
        }
    }

    pool->nlive_server = nlive_server;
    if (nlive_server == 0) 
    {
        LOG_DEBUG("no live servers for pool %" PRIu32 " '%.*s'", 
            pool->idx, pool->name.length(), pool->name.c_str());
        return NC_OK;
    }

    if (nlive_server > pool->nserver_continuum) 
    {
        uint32_t nserver_continuum = nlive_server + RANDOM_CONTINUUM_ADDITION;
        uint32_t ncontinuum = nserver_continuum * RANDOM_POINTS_PER_SERVER;

        for (uint32_t i = 0; i < (ncontinuum - pool->continuum.size()); i++)
        {
            pool->continuum.push_back(new NcContinuum());
        }

        srandom((uint32_t)time(NULL));
        pool->nserver_continuum = nserver_continuum;
    }

    uint32_t continuum_index = 0;
    uint32_t pointer_counter = 0;                           /* # pointers on continuum */
    uint32_t points_per_server, pointer_per_server;         /* points per server */
    for (uint32_t i = 0; i < nserver; i++)
    {
        NcServer *server = (pool->server)[i];
        if (pool->auto_eject_hosts && server->getNextRetry() > now) 
        {
            continue;
        }

        pointer_per_server = 1;

        pool->continuum[continuum_index]->index = i;
        pool->continuum[continuum_index]->value = 0;

        continuum_index++;

        pointer_counter += pointer_per_server;
    }

    LOG_DEBUG("updated pool %" PRIu32 " '%.*s' with %" PRIu32 " of "
        "%" PRIu32 " servers live in %" PRIu32 " slots "
        "active points in %" PRIu32 " slots", pool->idx,
        pool->name.length(), pool->name.c_str(), nlive_server, nserver,
        pool->nserver_continuum, 
        (pool->nserver_continuum + RANDOM_CONTINUUM_ADDITION) * points_per_server);

    return NC_OK;
}

rstatus_t NcHashKit::update(NcServerPool *pool)
{
    return random_update(pool);
}