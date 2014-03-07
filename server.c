#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#define UDP_BUFFER 4096
#define HEADER_LENGTH 12
#define DEFAULT_PORT 30000

#define PROC_DELIMITER ';'

#define TYPE_CPU 1
#define TYPE_MEM 2
#define TYPE_PROC 3
#define TYPE_IO 4

uint getUintFromChar(char * in);
int mysql_call(char * query);

int main(void) {
    int sockfd, n;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char mesg[UDP_BUFFER];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DEFAULT_PORT);
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    for (;;) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, mesg, UDP_BUFFER, 0, (struct sockaddr *)&cliaddr, &len);
        sendto(sockfd, mesg, n, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
        mesg[n] = 0;
        if (n > HEADER_LENGTH) {
            uint msg_len = getUintFromChar(mesg);
            uint msg_type = getUintFromChar(mesg + sizeof(uint));
            uint msg_time = getUintFromChar(mesg + 2 * sizeof(uint));
            char * msg_field = mesg + HEADER_LENGTH;

            switch(msg_type) {
                case TYPE_CPU: {
                    float cpu = 0;
                    sscanf(msg_field, "%f", &cpu);
                    char query[1000];
                    sprintf(query, "INSERT INTO cpu (`time`, `value`) VALUE(from_unixtime(\"%d\"), \"%f\")", msg_time, cpu);
                    mysql_call(query);
                    break;
                }
                case TYPE_MEM: {
                    float mem = 0;
                    float swap = 0;
                    sscanf(msg_field, "%f %f", &mem, &swap);
                    char query[1000];
                    sprintf(query, "INSERT INTO mem (`time`, `value`) VALUE(from_unixtime(\"%d\"), \"%f\")", msg_time, mem);
                    mysql_call(query);
                    sprintf(query, "INSERT INTO swap (`time`, `value`) VALUE(from_unixtime(\"%d\"), \"%f\")", msg_time, swap);
                    mysql_call(query);
                    break;
                }
                case TYPE_IO: {
                    uint io = 0;
                    sscanf(msg_field, "%d", &io);
                    char query[1000];
                    sprintf(query, "INSERT INTO io (`time`, `value`) VALUE(from_unixtime(\"%d\"), \"%d\")", msg_time, io);
                    mysql_call(query);
                    break;
                }
                case TYPE_PROC: {
                    int proc_cnt = 0;
                    int proc_ptr = 0;
                    int proc_ptr_cnt = 0;
                    int i = 0;
                    int j = 0;
                    char query[1000];

                    for (i = 0; i < msg_len; i++) {
                        if (msg_field[i] == PROC_DELIMITER) proc_cnt++;
                    }
                    char **proc_messages;
                    proc_messages = malloc(sizeof(char *) * proc_cnt);
                    
                    for (i = 0; i < msg_len; i++) {
                        if (msg_field[i] == PROC_DELIMITER || msg_field[i] == 0) {
                            int proc_len = i - proc_ptr;
                            proc_messages[proc_ptr_cnt] = malloc(sizeof(char) * (proc_len + 1));
                            int last_separator = 0;
                            for (j = 0; j < proc_len; j++) {
                                proc_messages[proc_ptr_cnt][j] = msg_field[proc_ptr + j];
                                if (' ' == msg_field[proc_ptr + j]) last_separator = j;
                            }
                            float value;
                            sscanf(proc_messages[proc_ptr_cnt] + last_separator, "%f", &value);
                            proc_messages[proc_ptr_cnt][last_separator] = 0;
                            char * proc_name = proc_messages[proc_ptr_cnt];

                            if (msg_field[i] != 0) proc_ptr_cnt++;
                            proc_messages[proc_ptr + proc_len] = 0;
                            proc_ptr = ++i;

                            sprintf(
                                query, 
                                "INSERT INTO proc (`time`, `proc`, `value`) VALUE(from_unixtime(\"%d\"), \"%s\", \"%f\")", 
                                msg_time, 
                                proc_name,
                                value
                            );
                            mysql_call(query);
                        };
                    }
                    break;
                }
            }
        }
    }

    return 0;
}

uint getUintFromChar(char * in) {
    uint result;
    memcpy(&result, in, sizeof(uint));
    return result;
}

int mysql_call(char * query) {
    char query_buf[1000];
    sprintf(query_buf, "mysql -uroot -p123 perf -e '%s'", query);
    int ret;
    ret = system(query_buf);
    return ret;
}
