#just listens for that one class

from multiprocessing.connection import Listener

address = ('localhost', 8989)
with Listener(address, authkey=b'1000011') as l:
    print("Scanner process started")
    while True:
        with l.accept() as conn:
            capture = conn.recv() 
            print(f"Time to clip: {capture.startTime} to {capture.endTime}. {capture.trigger} triggered it!")