#!/bin/bash

export pname=TMRAF

cd rt-n56u
git checkout .
git pull

cd ../$pname
git checkout .
git pull
chmod a+x scripts/*.sh
bash scripts/install_Opt.sh

cd ..
python3 $pname/tools/redirectOptimize.py rt-n56u O2 O3

cd rt-n56u/trunk
bash ../../$pname/scripts/rebuild_core.sh
cd ../..
