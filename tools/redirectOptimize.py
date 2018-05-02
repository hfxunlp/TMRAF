#encoding: utf-8

import os
import sys

def getcmd(src):
	ind = src.find("redirectOptimize.py")
	ld = src[:lind]
	t = src[lind:]
	ind = t.find(";")
	if ind == -1:
		rd = ""
		core = t
	else:
		rd = t[ind:]
		core = t[:ind]
	return ld, core, rd

def gncore(tin, old, new):
	tmp = tin.split(" ")
	tin[-1] = new
	tin[-2] = old
	return " ".join(tin)

def rebuildcmd(srcmd, ostr, nstr):
	l, core, r = getcmd(srcmd)
	return "".join((l, gncore(core, ostr, nstr), r,))

def rfile(fname, old, new):
	cache = []
	rwt = False
	try:
		with open(fname) as f:
			for line in f:
				tmp = line.strip("\r\n")
				tmp = tmp.decode("utf-8")
				if tmp.find("redirectOptimize.py") != -1:
					tmp = rebuildcmd(tmp, old, new)
					rwt = True
				else:
					if tmp.find(old) != -1:
						tmp = tmp.replace(old, new)
						rwt = True
				cache.append(tmp)
	except:
		rwt = False
	if rwt:
		while not cache[-1].strip():
			del cache[-1]
		cache.append("")
		cache = "\n".join(cache).encode("utf-8")
		print("edit:" + fname)
		with open(fname, "w") as f:
			f.write(cache)
	return rwt

def walkdir(dname, old, new):
	count = 0
	for root, dirs, files in os.walk(dname):
		for file in files:
			#if file == "Makefile":
			if file.lower().find("makefile") != -1:
				rp = rfile(os.path.join(root, file), old, new)
				if rp:
					count += 1
	if count > 0:
		print(str(count)+" files edited")

if __name__ == "__main__":
	walkdir(sys.argv[1].decode("utf-8"), sys.argv[2].decode("utf-8"), sys.argv[3].decode("utf-8"))
