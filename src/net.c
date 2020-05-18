#include "net.h"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sigchld_handler(int s) {
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

int32_t open_server_socket(char *port) {
	int32_t sockfd;
	struct addrinfo hints, *servinfo, *p;
	int32_t rv;
	struct sigaction sa;
	int yes = 1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		log_w("getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			log_w("error server: socket\n");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == 1) {
			log_w("error server: setsockopt\n");
			return -1;
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			log_w("server: bind\n");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if(p == NULL) {
		log_w("server: failed to bind\n");
		return -1;
	}

	if(listen(sockfd, CONN_BACKLOG) == -1) {
		log_w("server: err listen\n");
		return -1;
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		log_w("server: sigaction err\n");
		return -1;
	}

	return sockfd;	
}

int32_t open_socket(char *address, char *port) {
	int32_t sockfd;
	struct addrinfo hints, *servinfo, *p;
	int32_t rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
		log_w("getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			log_w("error client: socket\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			log_w("error client: connect\n");
			continue;
		}

		break;
	}

	if (p == NULL) {
		log_w("socket: failed to connect\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	log_w("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	return sockfd;
}
