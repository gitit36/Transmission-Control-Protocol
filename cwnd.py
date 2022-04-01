import pandas as pd
import matplotlib.pyplot as plt

cols=['time','cwnd'] 
cwnd = pd.read_csv('CWND.csv', names=cols, index_col=[0])
cwnd.plot()
plt.savefig('cwnd.png')