import os
import sys
import http.client
import urllib.parse

# Check for environment variables and arguments
if 'KELFSERVER_API_KEY' not in os.environ:
  print('Failed to encrypt: KELFSERVER_API_KEY is not set')
  sys.exit(1)

if 'KELFSERVER_API_ADDRESS' not in os.environ:
  print('Failed to encrypt: KELFSERVER_API_ADDRESS is not set')
  sys.exit(1)

if len(sys.argv) != 4:
    print('Failed to encrypt: invalid number of arguments')
    print('Usage: python3 script.py <header_id> <input_file> <output_file>')
    sys.exit(1)

header_id = sys.argv[1]
input_file_name = sys.argv[2]
output_file_name = sys.argv[3]

payload = open(input_file_name, 'rb').read()

# parse URL
url = urllib.parse.urlparse(os.environ['KELFSERVER_API_ADDRESS'] + '/encrypt?headerid={}'.format(header_id))

# Create HTTPS connection
conn = http.client.HTTPSConnection(url.netloc)

# Call KELFServer API
headers = {
    'X-Api-Key': os.environ['KELFSERVER_API_KEY'],
    'Content-Type': 'application/octet-stream',
}
conn.request('POST', url.path + '?' + url.query, body=payload, headers=headers)

response = conn.getresponse()

if response.status != 200:
    print('Failed to encrypt: HTTP {}'.format(response.status))
    sys.exit(1)

# write the response body to output file
open(output_file_name, 'wb').write(response.read())

print('File encrypted successfully')
