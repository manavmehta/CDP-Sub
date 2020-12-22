// Client.c
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <sys/epoll.h>
#include <pwd.h>

// #define Localhost
#define MAX 2000

using namespace std;

void send_string(string s, int fd)
{
    int t = 0;
    while (t < s.size())
    {
        int n = write(fd, s.substr(t).c_str(), s.size() - t);
        if (n < 0)
        {
            printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        t += n;
    }
}

pair<string, string> rec_message(int fd)
{
    char BUFF[MAX];

    string header = "";
    int t = 0;
    while (t < 12)
    {
        memset(BUFF, 0, MAX);
        int n = read(fd, BUFF, 12 - t);
        if (n < 0)
        {
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        t += n;
        header += BUFF;
    }
    header.erase(header.find('x'));

    string len = "";
    t = 0;
    while (t < 4)
    {
        memset(BUFF, 0, MAX);
        int n = read(fd, BUFF, 4 - t);
        if (n < 0)
        {
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        t += n;
        len += BUFF;
    }

    int len_of_payload = atoi(len.c_str());
    string payload = "";
    t = 0;
    while (t < len_of_payload)
    {
        memset(BUFF, 0, MAX);
        int n = read(fd, BUFF, len_of_payload - t);
        if (n < 0)
        {
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        t += n;
        payload += BUFF;
    }
    return {header, payload};
}

void send_message(string header, string payload, int fd)
{
    while (header.size() < 12)
    {
        header += 'x';
    }

    string len = to_string(payload.size());
    while (len.size() < 4)
    {
        len = '0' + len;
    }
    send_string(header + len + payload, fd);
}

void initial_connect(int fd)
{
    char *name;
    struct passwd *pass;
    pass = getpwuid(getuid());
    name = pass->pw_name;
    string userid;
    userid = name;
#if defined(Localhost)
    cout << "#> " << flush;
    cin >> userid;
#endif // Localhost

    send_message("myid", userid, fd);
    cout << rec_message(fd).second << endl;
}

string chat(int fd1, int fd2 = 0)
{
    struct epoll_event event1, event2, events[5];
    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        fprintf(stderr, "Failed to create epoll file descriptor\n");
        return "N";
    }

    event1.events = EPOLLIN;
    event1.data.fd = fd1;
    event2.events = EPOLLIN;
    event2.data.fd = fd2;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd1, &event1))
    {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd2, &event2))
    {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
    }

    getchar();
    while (true)
    {
        int event_count = epoll_wait(epoll_fd, events, 5, 3000);
        for (int i = 0; i < event_count; i++)
        {
            int fd = events[i].data.fd;
            string msg, header;
            if (fd == 0)
            {
                getline(cin, msg);
                header = "chat";
            }
            else
            {
                pair<string, string> p = rec_message(fd);
                header = p.first;
                msg = p.second;
            }
            if (header == "chat")
            {
                if (fd != fd1)
                {
                    send_message("chat", msg, fd1);
                }
                else
                {
                    cout << msg << endl;
                }
            }
            else
            {
                if (close(epoll_fd))
                {
                    fprintf(stderr, "Failed to close epoll file descriptor\n");
                }
                return msg;
            }
        }
    }
}

void client(int fd)
{
    while (1)
    {
        pair<string, string> p = rec_message(fd);
        string header = p.first;
        string msg = p.second;
        cout << msg << endl;
        if (header == "quit")
        {
            return;
        }
        else if (header == "chat")
        {
            cout << "starting chat ....\nuse @ to communicate with other user" << endl;
            string s = chat(fd);
            cout << s << endl;
            cout << "chat ended" << endl;
        }
        cout << "#> " << flush;
        string input;
        cin >> input;
        send_message("response", input, fd);
    }
}

// Server address
struct hostent *buildServerAddr(struct sockaddr_in *serv_addr,
                                char *serverIP, int portno)
{
    /* Construct an address for remote server */
    memset((char *)serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr->sin_family = AF_INET;
    inet_aton(serverIP, &(serv_addr->sin_addr));
    serv_addr->sin_port = htons(portno);
}

int main()
{
    //Client protocol
    char *serverIP = "127.0.0.1";
    int sockfd, portno = 5033;
    struct sockaddr_in serv_addr;

    buildServerAddr(&serv_addr, serverIP, portno);

    /* Create a TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* Connect to server on port */
    int n = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (n < 0)
    {
        printf("Oh dear, something went wrong with connect! %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Connected to %s:%d\n", serverIP, portno);
    initial_connect(sockfd);

    /* Carry out Client-Server protocol */
    client(sockfd);

    /* Clean up on termination */
    // close(sockfd);
}