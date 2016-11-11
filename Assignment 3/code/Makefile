SERVER_DIR = server
CLIENT_DIR = client

.PHONY: default

default:
	cp messages.h $(SERVER_DIR)
	cp messages.h $(CLIENT_DIR)
	$(MAKE) -C $(SERVER_DIR)
	$(MAKE) -C $(CLIENT_DIR)

client:
	cp messages.h $(CLIENT_DIR)
	$(MAKE) -C $(CLIENT_DIR)

server:
	cp messages.h $(SERVER_DIR)
	$(MAKE) -C $(SERVER_DIR)

