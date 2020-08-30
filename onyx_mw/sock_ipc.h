#include <vector>

typedef struct {
#ifdef WIN32
  SOCKET            sd;
#else
  int               sd;
#endif
  int               domain;
  struct sockaddr   *addr;
  int               addrlen;
  int               flags;
} spc_socket_t;

void spc_socket_close(spc_socket_t *sock);
spc_socket_t *spc_socket_listen(int type, int protocol, char *host, int port);
spc_socket_t *spc_socket_accept(spc_socket_t *sock);
spc_socket_t *spc_socket_connect(char *host, int port);
int spc_socket_send(spc_socket_t *sock, const void *buf, int buflen);
int spc_socket_recv(spc_socket_t *sock, void *buf, int buflen);
int spc_socket_recv_ready(spc_socket_t *sock, int nMillisecs);

typedef std::vector<spc_socket_t *> ClientSocketList_T;
spc_socket_t *spc_socket_set_recv_ready(ClientSocketList_T &sock_set, int nMillisecs);