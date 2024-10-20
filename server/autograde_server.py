#! /usr/bin/python3
import subprocess
import random
import time
import filecmp
import struct
import os
import shutil
import string
from ftplib import FTP

class TestServer:
    def __init__(self) -> None:
        self.credit = 0
        self.minor  = 2
        self.major  = 8
    
    def build(self):
        proc = subprocess.Popen('make', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        have_error = False
        while True:
            stdout = proc.stdout.readline()
            stderr = proc.stderr.readline()
            if not (stdout and stderr):
                break
            if stdout and '-Wall' not in stdout.decode("ascii"):
                print('No -Wall argument')
                print('Your credit is 0')
                exit(0)
            if stderr and not have_error:
                print('There are warnings when compiling your program')
                print(stderr)
                have_error = True
        if have_error:
            self.credit -= self.major

    def create_test_file(self, filename, filesize = 10000):
        f = open(filename, 'wb')
        for i in range(filesize):
            data = struct.pack('d', random.random())
            f.write(data)
        f.close()

    def test_public(self, port=21, directory='/tmp'):
        if port == 21 and directory == '/tmp':
            server = subprocess.Popen(['sudo', './server'], stdout=subprocess.PIPE)
        else:
            server = subprocess.Popen(['sudo', './server', '-port', '%d' % port, '-root', directory], stdout=subprocess.PIPE)
        time.sleep(0.1)
        try:
            ftp = FTP()
            # connect
            if not ftp.connect('127.0.0.1', port).startswith('220'):
                print('You missed response 220')
            else:
                self.credit += self.minor
            # login
            if not ftp.login().startswith('230'):
                print('You missed response 230')
            else:
                self.credit += self.minor
            # SYST
            if ftp.sendcmd('SYST') != '215 UNIX Type: L8':
                print('Bad response for SYST')
            else:
                self.credit += self.minor
            # TYPE
            if ftp.sendcmd('TYPE I') != '200 Type set to I.':
                print('Bad response for TYPE I')
            else:
                self.credit += self.minor

            # PORT download
            filename = 'test%d.data' % random.randint(100, 200)
            self.create_test_file(directory + '/' + filename)
            ftp.set_pasv(False)
            if not ftp.retrbinary('RETR %s' % filename, open(filename, 'wb').write).startswith('226'):
                print('Bad response for RETR')
            elif not filecmp.cmp(filename, directory + '/' + filename):
                print('Something wrong with RETR')
            else:
                self.credit += self.minor
            os.remove(directory + '/' + filename)
            os.remove(filename)
            
            # PASV upload
            ftp2 = FTP()
            ftp2.connect('127.0.0.1', port)
            ftp2.login()
            filename = 'test%d.data' % random.randint(100, 200)
            self.create_test_file(filename)
            if not ftp2.storbinary('STOR %s' % filename, open(filename, 'rb')).startswith('226'):
                print('Bad response for STOR')
            if not filecmp.cmp(filename, directory + '/' + filename):
                print('Something wrong with STOR')
            else:
                self.credit += self.minor
            os.remove(directory + '/' + filename)
            os.remove(filename)

            # QUIT
            if not ftp.quit().startswith('221'):
                print('Bad response for QUIT')
            else:
                self.credit += self.minor
            ftp2.quit()
            server.kill()

        except Exception as e:
            print('Exception occurred:', e)
        
        server.kill()

if __name__ == "__main__":
    test = TestServer()
    test.build()
    # Test 1
    test.test_public()
    # Test 2
    port = random.randint(2000, 3000)
    directory = ''.join(random.choice(string.ascii_letters) for x in range(10))
    if os.path.isdir(directory):
        shutil.rmtree(directory)
    os.mkdir(directory)
    test.test_public(port, directory)
    shutil.rmtree(directory)
    # Clean
    subprocess.run(['make', 'clean'], stdout=subprocess.PIPE)
    # Result
    if test.credit < 0: test.credit = 0
    print(f'Your credit is {test.credit}')