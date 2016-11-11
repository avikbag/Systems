SERVER_DIR = server
CLIENT_DIR = client

.PHONY: default

default:
	cp -r message/* $(CLIENT_DIR)/src
	cp -r message/* $(SERVER_DIR)
	$(MAKE) -C $(SERVER_DIR)
	$(MAKE) -C $(CLIENT_DIR)

client:
	$(MAKE) -C $(CLIENT_DIR)

server:
	$(MAKE) -C $(SERVER_DIR)

