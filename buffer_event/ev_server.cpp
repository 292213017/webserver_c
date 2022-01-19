#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>

void read_cb(struct bufferevent *bev, void *arg){
    char buf[1024] = {0};

    bufferevent_read(bev, buf, sizeof(buf));
    printf("client say: %s\n", buf);

    char *p = "rev the message!";

    bufferevent_write(bev, p , strlen(p) + 1);

    sleep(1);
}

void write_cb(struct bufferevent *bev, void *arg){
    printf("I'm server !");
}

void event_cb(struct bufferevent *bev, short events, void *arg){
    if(events & BEV_EVENT_EOF){
        printf("connetion close\n");
    }
    else if(events & BEV_EVENT_ERROR){
        printf("error\n");
    }
    bufferevent_free(bev);

    printf("free the bev \n");
}

void cb_listener(
    struct evconnlistener *listener,
    evutil_socket_t fd, 
    struct sockaddr *addr,
    int len , void *ptr
){
    printf("connect new client");

    struct event_base * base =  (struct event_base *)ptr;

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);

    bufferevent_enable(bev, EV_READ);
}

int main(int argc, char **argv)
{
    struct sockaddr_in serv;

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(9876);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    struct event_base *base = event_base_new();

    // struct bufferevent *bev = bufferevent_socket_new();

    struct evconnlistener *listener = evconnlistener_new_bind(base, cb_listener, base,
                                                              LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 36, (struct sockaddr *)&serv, sizeof(serv));

    event_base_dispatch(base);

    evconnlistener_free(listener);

    event_base_free(base);

    return 0;
}