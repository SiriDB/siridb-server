/*
 * socket.c - Handle TCP request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/net/socket.h>
#include <siri/siri.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOWED_PKG_SIZE 20971520  // 20 MB
#define SUGGESTED_SIZE 65536

static sirinet_socket_t * SOCKET_new(sirinet_socket_tp_t tp, on_data_cb_t cb);

const char * sirinet_socket_ip_support_str(uint8_t ip_support)
{
	switch (ip_support)
	{
	case IP_SUPPORT_ALL: return "ALL";
	case IP_SUPPORT_IPV4ONLY: return "IPV4ONLY";
	case IP_SUPPORT_IPV6ONLY: return "IPV6ONLY";
	default: return "UNKNOWN";
	}
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_socket_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) handle->data;

    if (ssocket->buf == NULL)
    {
        buf->base = (char *) malloc(SUGGESTED_SIZE);
        if (buf->base == NULL)
        {
            ERR_ALLOC
            buf->len = 0;
        }
        else
        {
            buf->len = SUGGESTED_SIZE;
        }
    }
    else
    {
    	suggested_size = (ssocket->len > sizeof(sirinet_pkg_t)) ?
			((sirinet_pkg_t *) ssocket->buf)->len + sizeof(sirinet_pkg_t) :
			SUGGESTED_SIZE;

        buf->base = ssocket->buf + ssocket->len;
        buf->len = suggested_size - ssocket->len;
    }
}

/*
 * Buffer should have a size of ADDR_BUF_SZ
 *
 * Return 0 if successful or -1 in case of an error.
 */
int sirinet_addr_and_port(char * buffer, uv_stream_t * client)
{
    struct sockaddr_storage name;
    int namelen = sizeof(name);

    if (uv_tcp_getpeername(
            (uv_tcp_t *) client,
            (struct sockaddr *) &name,
            &namelen))
    {
        return -1;
    }

    switch (name.ss_family)
    {
    case AF_INET:
    	{
    		char addr[INET_ADDRSTRLEN];
            uv_inet_ntop(
            		AF_INET,
					&((struct sockaddr_in *) &name)->sin_addr,
					addr,
					sizeof(addr));
            snprintf(
            		buffer,
					ADDR_BUF_SZ,
					"%s:%d",
					addr,
					ntohs(((struct sockaddr_in *) &name)->sin_port));
    	}
        break;

    case AF_INET6:
    	{
    		char addr[INET6_ADDRSTRLEN];
			uv_inet_ntop(
					AF_INET6,
					&((struct sockaddr_in6 *) &name)->sin6_addr,
					addr,
					sizeof(addr));
			snprintf(
					buffer,
					ADDR_BUF_SZ,
					"[%s]:%d",
					addr,
					ntohs(((struct sockaddr_in6 *) &name)->sin6_port));
    	}
    	break;

    default:
    	return -1;
    }

    return 0;
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_socket_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) client->data;
    sirinet_pkg_t * pkg;
    size_t total_sz;

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }

        /* the buffer must be destroyed too
         * (sirinet_socket_free will free ssocket->buf if needed)
         */
        if (ssocket->buf == NULL)
        {
            free(buf->base);
        }

        sirinet_socket_decref(client);
        return;
    }

    if (ssocket->buf == NULL)
    {
        if (nread >= sizeof(sirinet_pkg_t))
        {
            pkg = (sirinet_pkg_t *) buf->base;

            if (    (pkg->tp ^ 255) != pkg->checkbit ||
                    (ssocket->tp == SOCKET_CLIENT &&
                            pkg->len > MAX_ALLOWED_PKG_SIZE))
            {
                log_error(
                        "Got an illegal package or size too large, "
                        "closing connection (pid: %lu, len: %lu, tp: %u)",
                        pkg->pid, pkg->len, pkg->tp);
                free(buf->base);

                sirinet_socket_decref(client);
                return;
            }

            total_sz = pkg->len + sizeof(sirinet_pkg_t);

            if (nread >= total_sz)
            {
                /* Call on-data function */
                (*ssocket->on_data)(client, pkg);

                if (nread == total_sz)
                {
                    free(buf->base);
                }
                else
                {
                    /*
                     * move rest data to start of buffer and call this function
                     * again.
                     */
                    memmove(buf->base, buf->base + total_sz, nread - total_sz);
                    sirinet_socket_on_data(client, nread - total_sz, buf);
                }

                return;
            }

            /* total size > 0 */
            ssocket->buf = (buf->len < total_sz) ?
                (char *) realloc(buf->base, total_sz) : buf->base;

            if (ssocket->buf == NULL)
            {
                log_critical(
                        "Cannot allocate size for package "
                        "(pid: %lu, len: %lu, tp: %u)",
                        pkg->pid, pkg->len, pkg->tp);
                free(buf->base);
                return;
            }
        }
        else
        {
            ssocket->buf = buf->base;
        }

        ssocket->len = nread;

        return;
    }

    if (ssocket->len < sizeof(sirinet_pkg_t))
    {
        ssocket->len += nread;

        if (ssocket->len < sizeof(sirinet_pkg_t))
        {
            return;
        }

        pkg = (sirinet_pkg_t *) ssocket->buf;

        if (    (pkg->tp ^ 255) != pkg->checkbit ||
                (ssocket->tp == SOCKET_CLIENT &&
                        pkg->len > MAX_ALLOWED_PKG_SIZE))
        {
            log_error(
                    "Got an illegal package or size too large, "
                    "closing connection (pid: %lu, len: %lu, tp: %u)",
                    pkg->pid, pkg->len, pkg->tp);
            sirinet_socket_decref(client);
            return;
        }

        total_sz = pkg->len + sizeof(sirinet_pkg_t);

        if (buf->len < total_sz)
        {
            /* total sz > 0 */
            char * tmp = (char *) realloc(ssocket->buf, total_sz);

            /* test re-allocation */
            if (tmp == NULL)
            {
                log_critical(
                        "Cannot allocate size for package "
                        "(pid: %lu, len: %lu, tp: %u)",
                        pkg->pid, pkg->len, pkg->tp);
                free(ssocket->buf);
                ssocket->buf = NULL;
                return;
            }

            /* bind the new allocated buffer */
            ssocket->buf = tmp;

            /*
             * Pkg is already checked in this case but we need to bind it
             * to the re-allocated buffer
             */
            pkg = (sirinet_pkg_t *) ssocket->buf;
        }
    }
    else
    {
        ssocket->len += nread;

        /* pkg is already checked in this case */
        pkg = (sirinet_pkg_t *) ssocket->buf;

        total_sz = pkg->len + sizeof(sirinet_pkg_t);
    }

    if (ssocket->len < total_sz)
    {
        return;
    }

    if (ssocket->len > total_sz)
    {
        /* Call on-data function. */
        (*ssocket->on_data)(client, pkg);

        ssocket->len -= total_sz;
        memmove(ssocket->buf, ssocket->buf + total_sz,  ssocket->len);

        /*
         * The ssocket->buf now has ssocket->len size. it might be possible
         * the we do not have enough to determine the next package size, we
         * can have the next package complete, partly or even multiple
         * packages are possible.
         */
        char * tmp = (char *) realloc(ssocket->buf,
			(ssocket->len < sizeof(sirinet_pkg_t) ||
			(total_sz = ((sirinet_pkg_t *) ssocket->buf)->len +
					sizeof(sirinet_pkg_t)) < SUGGESTED_SIZE) ?
				SUGGESTED_SIZE : total_sz);

        if (tmp == NULL)
        {
            log_critical("Cannot allocate size for buffer");
            free(ssocket->buf);
            ssocket->buf = NULL;
            return;
        }
        ssocket->buf = tmp;

        /* call this function again with rest data */
        sirinet_socket_on_data(client, 0, buf);

        return;
    }

    /* Call on-data function. */
    (*ssocket->on_data)(client, pkg);

    free(ssocket->buf);
    ssocket->buf = NULL;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Note: ((sirinet_socket_t *) socket->data)->ref is initially set to 1
 */
uv_tcp_t * sirinet_socket_new(sirinet_socket_tp_t tp, on_data_cb_t cb)
{
    uv_tcp_t * socket = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    if (socket == NULL)
    {
        ERR_ALLOC
    }
    else if ((socket->data = SOCKET_new(tp, cb)) == NULL)
    {
        free(socket);
        socket = NULL;  /* signal is raised */
    }
    return socket;
}

/*
 * Never use this function but call sirinet_socket_decref.
 * Destroy socket. (parsing NULL is not allowed)
 *
 * We know three different socket types:
 *  - client: used for clients. a user object might be destroyed.
 *  - back-end: used to connect to other servers. a server might be destroyed.
 *  - server: user for severs connecting to here. a server might be destroyed.
 *
 *  In case a server is destroyed, remaining promises will be cancelled and
 *  the call-back functions will be called.
 */
void sirinet__socket_free(uv_stream_t * client)
{
    sirinet_socket_t * ssocket = client->data;

#ifdef DEBUG
    log_debug("Free socket type: %d", ssocket->tp);
#endif

    switch (ssocket->tp)
    {
    case SOCKET_CLIENT:
        if (ssocket->origin != NULL)
        {
        	siridb_user_t * user = (siridb_user_t *) ssocket->origin;
            siridb_user_decref(user);
        }
        break;
    case SOCKET_BACKEND:
        if (ssocket->origin != NULL)
        {
        	siridb_server_t * server = (siridb_server_t *) ssocket->origin;
            siridb_server_decref(server);
        }
        break;
    case SOCKET_SERVER:
        {
            siridb_server_t * server = ssocket->origin;
            server->socket = NULL;
            server->flags = 0;
            siridb_server_decref(server);
        }
    }
    free(ssocket->buf);
    free(ssocket);
    free((uv_tcp_t *) client);
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (reference counter is initially set to 1)
 */
static sirinet_socket_t * SOCKET_new(sirinet_socket_tp_t tp, on_data_cb_t cb)
{
    sirinet_socket_t * ssocket =
            (sirinet_socket_t *) malloc(sizeof(sirinet_socket_t));
    if (ssocket == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        ssocket->tp = tp;
        ssocket->on_data = cb;
        ssocket->buf = NULL;
        ssocket->len = 0;
        ssocket->origin = NULL;
        ssocket->siridb = NULL;
        ssocket->ref = 1;
    }
    return ssocket;
}




