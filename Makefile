all: 
	gcc shm_server.c -o shm_server -Wall -Werror -lrt -lpthread
	gcc shm_client.c -o shm_client -Wall -Werror -lrt -lpthread
clean:
	rm shm_client
	rm shm_server
