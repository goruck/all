#!/bin/bash
echo "starting zip"
zip all.zip all.js shared.js host.txt port.txt ca.crt client.crt client.key
zip -r all.zip amzn/
echo "saved files to all.zip"