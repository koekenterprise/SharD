#!/bin/bash

SCRIPT_NAME="shard"
INSTALL_PATH="/usr/local/bin/"

echo "SharD INSTALLER"

if [ ! -f "$SCRIPT_NAME" ]; then
    echo "Error: Couldn't find the file 'shard'."
    exit 1
fi

echo "*Initializing Instalation Process . . .* "
chmod +x "$SCRIPT_NAME"
sudo mv "$SCRIPT_NAME" "$INSTALL_PATH"

if [ $? -eq 0 ]; then
	echo "* Done! SharD  Configured Sucesfully!"
else
	echo "* Error: Couldn't give executable permission nor move the shard  file. *"

fi
