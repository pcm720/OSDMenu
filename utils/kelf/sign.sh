#!/bin/bash

# A simple script that calls the kelfserver API to sign the KELF
# Requires KELFSERVER_API_ADDRESS and KELFSERVER_API_KEY to be defined in the environment
# Usage: sign.sh header_id input_file output_file

# Check for environment variables and arguments
if [[ -z "${KELFSERVER_API_KEY}" ]]; then
  echo "Failed to encrypt: KELFSERVER_API_KEY is not set"
  exit 1
fi

if [[ -z "${KELFSERVER_API_ADDRESS}" ]]; then
  echo "Failed to encrypt: KELFSERVER_API_ADDRESS is not set"
  exit 1
fi

if [ "$#" -ne 3 ]; then
    echo "Failed to encrypt: invalid number of arguments"
    echo "Usage: $0 header_id input_file output_file"
    exit 1
fi

HEADERID=$1
INPUT_FILENAME=$2
OUTPUT_FILENAME=$3

# Call the kelfserver API
HTTP_STATUS=$(curl -s -o $OUTPUT_FILENAME -w "%{http_code}" -X POST "${KELFSERVER_API_ADDRESS}/encrypt?headerid=${HEADERID}" \
  -H "X-Api-Key: ${KELFSERVER_API_KEY}" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@${INPUT_FILENAME}")

if [ "$HTTP_STATUS" -ne 200 ]; then
  echo "Failed to encrypt: HTTP $HTTP_STATUS"
  exit 1
fi

echo "File encrypted successfully"
