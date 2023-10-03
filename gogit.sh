#!/bin/sh

# change .git/config:
#        #url = https://github.com/Alzymologist/wasm-js.git
#        url = git@github.com:Alzymologist/wasm-js.git


#git clone git@github.com:Alzymologist/shave-rust.git
git status
git add .
git commit -m "major: init"
git push
