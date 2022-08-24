"""
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
"""

from flask import Flask
from flask import send_file

import numpy as np
import pandas
import random

import matplotlib.pyplot as plt

import posix_ipc as ipc
import mmap

app = Flask(__name__)

#Runs when the user goes to our ip address
@app.route('/')
def histapp():

	#Create and save the figure
	create_hist()

	#Send the histogram as a webpage to the user
	return send_file("/root/hist.png", mimetype='image/png')

#Creates the user histogram and saves to hist.png
def create_hist():

	#Go to the beginning of the shared memory region
	shm.seek(0)

	#Get the data
	web_sem.acquire()
	data = shm.readline();
	web_sem.release()

	#Transform the data into an array that matplotlib can understand
	count, dataset = transform_data(data)

	#Clear the figure and recreate from the new data
	plt.clf()

	#Setup the figure and save it
	bins=np.arange(min(dataset),max(dataset)+2)-0.5
	figure = plt.hist(dataset,bins,rwidth=0.5)

	plt.title("ACRN Sample Application cyclictest display (unoptimized)")
	plt.xlabel("Latency Value (microseconds)")
	plt.ylabel("Count Percentage      " + f"{count:,}")
	plt.savefig("/root/hist.png")

	return figure

def transform_data(data_string):

	#Holds the transformed data
	transformed_data_values = []

	str_data = data_string.decode("utf-8")
	str_data = str_data.replace('\n',"")

	data_values = str_data.split()

	#Holds the count of latencies that we have
	total_count = data_values[0]

	#Used for transforming the data values
	data_percentages = data_values[1:]
	if (len(data_percentages) % 2 != 0):
		return transformed_data_values

	#Transform the data into a list that can be fed to matplotlib
	for i in range(0, int(len(data_percentages) / 2)):
		transformed_data_values += ([int(data_percentages[i*2])] * int(data_percentages[i*2 + 1]))

	return int(total_count), transformed_data_values

if __name__ == '__main__':

	#Set up shared memory between userapp and the webserver
	shm_path = "/pyservershm"
	shm_size = 1048576
	shm_f =  ipc.SharedMemory(shm_path, 0, size=shm_size)
	shm = mmap.mmap(shm_f.fd, shm_size)

	#Set up the semaphore to maintaine synchronization
	web_sem = ipc.Semaphore("/pyserversem", 0, 0o0774)

	#Run the webserver
	app.run(host="0.0.0.0", port=80, debug=False)
