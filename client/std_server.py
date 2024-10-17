import sys
import logging
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

if __name__ == "__main__":
    # logging.basicConfig(filename="test.log", level=logging.INFO)
    root_dir = '/tmp'
    port = 10021
    if len(sys.argv) >= 2: root_dir = sys.argv[1]
    if len(sys.argv) >= 3: port = int(sys.argv[2])
    
    authorizer = DummyAuthorizer()
    authorizer.add_user("anonymous", "anonymous", root_dir, perm="elradfmw")
    
    handler = FTPHandler
    handler.authorizer = authorizer
    # handler.log = logging.info
    
    server = FTPServer(("127.0.0.1", port), handler)
    
    server.serve_forever()