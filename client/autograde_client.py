import subprocess
import os
import sys
import time
import queue
import signal
import random
import string
import struct
import shutil
import filecmp
import threading
from io import StringIO
from multiprocessing import Process
from ftplib import FTP


def read_client_output(client, output_queue: queue.Queue):
    while True:
        line = client.stdout.readline()
        if not line:
            break
        line = line.strip()
        if line: output_queue.put(line)


def create_test_file(filename, filesize = 10000):
    f = open(filename, 'wb')
    for _ in range(filesize):
        data = struct.pack('d', random.random())
        f.write(data)
    f.close()


class TestClient:
    def __init__(self, server_root_dir="/tmp", port=10021):
        self.logfilename = "test.log"
        self.server = None
        self.client = None
        self.reading_thread = None
        self.client_output_queue = queue.Queue()
        self.server_root_dir = server_root_dir
        self.server_port = port
        self.new_dir = False
        if not os.path.exists(self.server_root_dir):
            self.new_dir = True
            os.makedirs(self.server_root_dir)


    def __del__(self):
        if os.path.exists(self.logfilename):
            os.remove(self.logfilename)
        if self.new_dir and os.path.exists(self.server_root_dir):
            # os.rmdir(self.server_root_dir)
            shutil.rmtree(self.server_root_dir)


    # run a standard FTP server
    def run_std_server(self):
        with open(self.logfilename, "w") as logfile:
            self.server = subprocess.Popen(["python", "std_server.py", self.server_root_dir, str(self.server_port)],
                                            stdin=subprocess.PIPE,
                                            stdout=logfile,
                                            stderr=logfile,
                                            text=True)
        return self.server


    # run your clinet
    def run_client(self):
        self.client = subprocess.Popen(["./client", "-ip", "127.0.0.1", "-port", "10021"],
                                        stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE,
                                        bufsize=1,
                                        universal_newlines=True,
                                        text=True)
        return self.client
    
    
    def check_server_output(self, expected_output):
        with open(self.logfilename) as logfile:
            server_output = logfile.readlines()[-1].strip()
            if server_output.endswith(expected_output):
                return True
            else:
                print("Failed")
                print("Expected: ", expected_output)
                print("Got: ", server_output)
                return False


    def check_client_output(self, expected_output):
        if self.client_output_queue.empty():
            return False
        client_output = self.client_output_queue.get(timeout=1)
        self.client_output_queue.task_done()

        if client_output.startswith(expected_output):
            return True
        else:
            print("Failed")
            print("Expected: ", expected_output)
            print("Got: ", client_output)
            return False
    
    
    def test_syst(self):
        self.client.stdin.write("SYST\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        return self.check_client_output("215")
    
    
    def test_type(self):
        self.client.stdin.write("TYPE I\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        return self.check_client_output("200")


    def test_mkd(self, dirname):
        self.client.stdin.write(f"MKD {dirname}\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        expected_server_return = f"[anonymous] MKD {self.server_root_dir}/{dirname} 257"
        expected_client_return = f"257 \"/{dirname}\" directory created."
        return self.check_server_output(expected_server_return) and \
            self.check_client_output(expected_client_return)


    def test_cwd(self, dirname, pathname):
        self.client.stdin.write(f"CWD {dirname}\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        if pathname == '/':
            expected_server_return = f"[anonymous] CWD {self.server_root_dir} 250"
        else:
            expected_server_return = f"[anonymous] CWD {self.server_root_dir}{pathname} 250"
        expected_client_return = f"250 \"{pathname}\" is the current directory."
        return self.check_server_output(expected_server_return) and \
            self.check_client_output(expected_client_return)
            
            
    def test_pwd(self, pathname):
        self.client.stdin.write("PWD\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        expected_client_return = f"257 \"{pathname}\" is the current directory."
        return self.check_client_output(expected_client_return)


    def test_pasv(self):
        self.client.stdin.write("PASV\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        if not self.check_client_output("227"):
            print("PASV failed")
            return False
        return True
    
    
    def test_port(self):
        random_port = random.randint(20000, 65536)
        req = f"PORT 127,0,0,1,{random_port//256},{random_port%256}\n"
        self.client.stdin.write(req)
        self.client.stdin.flush()
        time.sleep(0.1)
        
        if not self.check_client_output("200"):
            print("PORT failed")
            return False
        return True
    
    
    def test_retr(self, filename, pathname, filesize=10000):
        create_test_file(pathname + '/' + filename, filesize)
        
        self.client.stdin.write(f"RETR {filename}\n")
        self.client.stdin.flush()
        time.sleep(1)
        
        client_output_list = []
        while not self.client_output_queue.empty():
            client_output_list.append(self.client_output_queue.get(timeout=1))
            self.client_output_queue.task_done()
        if len(client_output_list) < 2:
            print("Bad response for RETR")
            os.remove(filename)
            os.remove(pathname+'/'+filename)
            return False
        if not ((client_output_list[0].startswith("150") or client_output_list[0].startswith("125")) and\
                client_output_list[-1].startswith("226")):
            print("Bad response for RETR")
            os.remove(filename)
            os.remove(pathname+'/'+filename)
            return False
        if not filecmp.cmp(filename, pathname+'/'+filename):
            print("Something wrong with RETR")
            os.remove(filename)
            os.remove(pathname+'/'+filename)
            return False
        
        os.remove(filename)
        os.remove(pathname+'/'+filename)
        return True


    def test_login(self):
        expected_server_return = "[anonymous] USER 'anonymous' logged in."
        # USER
        self.client.stdin.write("USER anonymous\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        expectecd_client_return = "331"
        if not self.check_client_output(expectecd_client_return):
            return False
        
        # PASS
        self.client.stdin.write("PASS anonymous\n")
        self.client.stdin.flush()
        time.sleep(0.1)
        
        expectecd_client_return = "230"
        return self.check_client_output(expectecd_client_return) and \
            self.check_server_output(expected_server_return)

    
    def test_part(self):
        directory_name = "test_" + ''.join(random.choice(string.ascii_letters) for _ in range(10))
        credit = 0
        
        # TYPE & SYST
        credit += 1 if self.test_syst() else 0
        credit += 1 if self.test_type() else 0
        
        # MKD
        credit += 1 if self.test_mkd(directory_name) else 0
        
        # CWD & PWD
        credit += 1 if self.test_cwd(dirname = directory_name, pathname="/"+directory_name) else 0
        credit += 1 if self.test_pwd("/"+directory_name) else 0
        
        # PORT & RETR
        credit += 2 if self.test_port() else 0
        credit += 4 if self.test_retr("test_retr.data", self.server_root_dir+'/'+directory_name) else 0
        
        # PASV & RETR
        credit += 2 if self.test_pasv() else 0
        credit += 4 if self.test_retr("test_retr_2.data", self.server_root_dir+'/'+directory_name) else 0
        
        return credit
    
    
    def test_public(self):
        # run a standard FTP server
        self.run_std_server()
        time.sleep(1)
        # run your client
        self.run_client()
        # reading client's output
        self.reading_thread = threading.Thread(target=read_client_output, args=(self.client, self.client_output_queue))
        self.reading_thread.start()
        time.sleep(1)
        
        credit = 0
        credit -= 2 if not self.check_client_output("220") else 0
        
        # testcases
        try:
            if self.test_login():
                credit += 5
                credit += self.test_part()
                # print(credit)
        except Exception as e:
            print("Exception occured: ", e)
            credit = 0
        finally:
            self.client.terminate()
            self.server.terminate()
            self.reading_thread.join()
        credit = 0 if credit < 0 else credit
        print(f"Your client credit is {credit}")


if __name__=="__main__":
    test = TestClient(server_root_dir=os.getcwd()+"/client_test", port=10021)
    test.test_public()
