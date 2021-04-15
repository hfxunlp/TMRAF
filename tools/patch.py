#encoding: utf-8

import sys
from os import walk
from os.path import join as pjoin

def apply_patch(line, rules):

	patched = 0
	rs = line
	for fstr, src, tgt in rules:
		if fstr is None or line.find(fstr) >= 0:
			if line.find(src) >= 0:
				rs = rs.replace(src, tgt)
				patched += 1

	return patched, rs

def patch_decoder(fname):

	patched = 0

	prules = ((None, "/media/Storage/workspace/router/", "/home/anoliu/router/",),)

	cache = []
	prev_emp = False
	with open(fname, "rb") as f:
		for line in f:
			tmp = line.rstrip()
			if tmp:
				_patched, tmp = apply_patch(tmp.decode("utf-8"), prules)
				patched += _patched
				cache.append(tmp)
				prev_emp = False
			else:
				if not prev_emp:
					cache.append(tmp.decode("utf-8"))
				prev_emp = True
	while not cache[-1]:
		del cache[-1]
	with open(fname, "wb") as f:
		f.write("\n".join(cache).encode("utf-8"))
		f.write("\n".encode("utf-8"))

	return patched

def walk_path(ptws):

	for ptw in ptws:
		for root, dirs, files in walk(ptw):
			for execf in files:
				if ((execf.lower().find("makefile") >=0) or execf.endswith(".config")) and execf.find("patch") < 0:
					_execf = pjoin(root, execf)
					#print("Processing %s" % (_execf))
					if patch_decoder(_execf) > 0:
						print("Patched %s" % (_execf))

if __name__ == "__main__":
	walk_path(sys.argv[1:])
