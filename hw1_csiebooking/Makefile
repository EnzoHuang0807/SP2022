all: write_server read_server 

write_server: server.c 
	gcc $^ -D WRITE_SERVER -o $@	

read_server: server.c 
	gcc $^ -D READ_SERVER -o $@	
clean:
	rm -f read_server write_server 

