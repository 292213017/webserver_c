#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <event2/event.h>

using namespace std;

void sys_err(const char *str){
    perror(str);
    exit(1);
}

int main(int argc, char *argv[]){
    // 创建base
    struct event_base *base = event_base_new();

    const char **buf;

    //支持的IO
    // buf = event_get_supported_methods();
    buf = event_base_get_methods(base);
    for(int i = 0; i < 10; i++){
        cout << buf[i] << endl;
    }
    return 0;
}