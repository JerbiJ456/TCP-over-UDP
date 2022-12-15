import subprocess
import os 
import matplotlib.pyplot as plt 


for i in range (10,301,10):
    subprocess.Popen(["./bin/server1-Thehunters", "5050",str(i)])
    p2 = subprocess.Popen(["./bin/client2" ,"0.0.0.0" ,"5050" ,"1MB" ,"0"])
    p2.wait()

x = []
y =[]

f = open ("client1.txt","r")
for i in f.readlines():
    line = i.split(";")
    yv = line[1].split(".")
    print(float(yv[1])/((len(yv[1])-1)*10), ((len(yv[1])-1)*10))
    yv = float(yv[0]) + float(yv[1])/(10**(len(yv[1])-1))  
    print(yv)
    x.append(round(int(line[0]),2))
    y.append(yv)

plt.plot(x, y)
plt.xticks([i for i in range(0,501,50)])
plt.yticks([i for i in range(0,10)])

# Add labels and a title
plt.xlabel('Size Fenêtre')
plt.ylabel('Débit')
plt.title('Le débit en fonction de la taille de la fenêtre Client1')

# Save the plot as an image
plt.savefig('plot.png')
