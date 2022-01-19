#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#define MAXSIZE  2048

char *get_mime_type(const char *name)
{
    char* dot;

    dot = strrchr(name, '.');	//自右向左查找‘.’字符;如不存在返回NULL
    /*
     *charset=iso-8859-1	西欧的编码，说明网站采用的编码是英文；
     *charset=gb2312		说明网站采用的编码是简体中文；
     *charset=utf-8			代表世界通用的语言编码；
     *						可以用到中文、韩文、日文等世界上所有语言编码上
     *charset=euc-kr		说明网站采用的编码是韩文；
     *charset=big5			说明网站采用的编码是繁体中文；
     *
     *以下是依据传递进来的文件名，使用后缀判断是何种文件类型
     *将对应的文件类型按照http定义的关键字发送回去
     */
    printf("file name is %s\n", dot);
    if (dot == (char*)0)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

int get_line(int cfd, char *buf, int size){
    int i = 0;
    char c = '\0';
    int n = 0;
    while((i < size - 1) && (c != '\n')){
        n = recv(cfd, & c, 1, 0);
        if(n > 0){
            if(c == '\r'){
                n =recv(cfd, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n')){
                    recv(cfd, &c, 1, 0);
                }else{
                    c = '\n';
                }
            }
            buf[i] = c;
            i ++;
        }else{
            c ='\n';
        }
    }
    buf[i] = '\0';
    if( - 1 == n)
        i = n;
    return i;
}

void disconnect(int cfd, int epfd){
        int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        if(ret != 0){
            perror("epoll ctl failed");
            exit(1);
        }
        close(cfd);
}

//client df, error num, error discripter, type and len
void send_respond(int cfd, int no, char *disp, char *type, int len){
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    send(cfd, buf,strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);
}

//send local filt to client
void send_file(int cfd , const char *file){
    // printf("File: %s\r\n", file);
    int ret;
    char buf[4096] = {0};
    int n = 0;
    int fd = open(file, O_RDONLY);
    
    if(fd == -1){
        perror("open error");
        exit(1);
    }

    while((n = read(fd, buf, sizeof(buf))) > 0){
        ret = send(cfd, buf, n, 0);
        if(ret == -1){
            printf("errno = %d\n", errno);
            if(errno == EAGAIN){
                printf("-----------EAGAIN\n");
                continue;
            }else if(errno == EINTR){
                printf("--------EINTR\n");
                continue;
            }else{
                perror("send error");
                exit(1);
            }
        }
        if(ret < 4096){
            printf("-----------send ret : %d\n", ret);
        }
    }
    close(fd);
}

int init_listen_fd(int port, int epfd){
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1){
        perror("socket error");
        exit(1);
    }

    struct sockaddr_in srv_addr;
    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int ret = bind(lfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    if(ret == -1) {
        perror("bind error");
        exit(1);
    }

    ret = listen(lfd, 128);
    if(ret == -1) {
        perror("l listen error");
        exit(1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1){
        perror("ctl error");
        exit(1);
    }

    return lfd;

}

void do_accept(int lfd, int epfd){
    struct sockaddr_in clt_addr;
    socklen_t addrlen = sizeof(clt_addr);

    int cfd = accept(lfd, (struct sockaddr *)&clt_addr, &addrlen);
    if(cfd == -1){
        perror("accept error");
        exit(1);
    }

    char client_ip[64] = {0};
    printf("client ip: %s, Port %d, cfd %d\n", inet_ntop(AF_INET, &clt_addr.sin_addr.s_addr,client_ip, sizeof(client_ip)), ntohs(clt_addr.sin_port), cfd);

    int flag = fcntl(cfd, F_GETFL); 
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    struct epoll_event ev;
    ev.data.fd = cfd;

    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if(ret == -1){
        perror("epoll ctl");
        exit(1);
    }

}

//处理http请求
void http_request(int cfd, const char *file){
    struct stat sbuf;

    int ret = stat(file, &sbuf);
    if(ret != 0){
        // 回发浏览器404;
        perror("stat \n");
        exit(1);
    }
    if(S_ISREG(sbuf.st_mode)){
        //is a regular file
        printf("--------------is a regular file\n");
        //call back http request
        send_respond(cfd, 200 , "OK",get_mime_type(file) , sbuf.st_size);
        //call back the client side
        send_file(cfd, file);
        // printf("there is ok \n");
    }
}

void do_read(int cfd, int epfd){
    char line[1024] = {0};
    char method[16], path[256],protocal[16];
    int len =  get_line(cfd, line, sizeof(line));  //读取http请求协议首行 GET 、hello.c HTTP/1.1

    if(len == 0){
        printf("server find the client is closed\n");
        disconnect(cfd, epfd);
    }else{

        sscanf(line, "%[^ ] %[^ ] %[^ ]",method, path, protocal);
        printf("method = %s, path = %s, protocol = %s \n", method, path, protocal);

        while(1){
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if(buf[0] == '\n'){
                break;
            } else if(len == -1){
                break;
            }
            // printf("%d",len);
            printf("---------------%s\n", buf);
            // sleep(2);
        }
        if(strncasecmp(method, "GET", 3) == 0){
            //客户端要的文件名
            char *file = path + 1;     
            printf("file name is %s\n", file); 
            http_request(cfd, file);
            // 这里要记得断开
            disconnect(cfd, epfd);
        }
    }
}

void epoll_run(int port){
    struct epoll_event all_events[MAXSIZE];

    // 创建监听树
    int epfd = epoll_create(MAXSIZE);
    if(epfd == -1){
        perror("epoll_create error");
        exit(1);
    }

    // 创建监听lfd，并且封装到树
    int lfd = init_listen_fd(port, epfd);

    while(1){
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if(ret == -1){
            perror("epoll_wait error");
            exit(1);
        }

        for(int i = 0; i < ret ; ++ i){
            struct epoll_event *pev = &all_events[i];
            if(!(pev->events & EPOLLIN)){
                continue;
            }
            if(pev->data.fd == lfd){
                do_accept(lfd, epfd);
            }else{
                do_read(pev->data.fd, epfd);
            }
        }
    }
}

int main(int argc, char **argv){
    if(argc < 3){
        printf("./server post path\n");
    }
    int port = atoi(argv[1]);

    //改变工作目录
    int ret = chdir(argv[2]);
    if(ret != 0){
        perror("chdir error");
        exit(1);
    }

    epoll_run(port);
    return 0;
}