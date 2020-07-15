#!/bin/sh
set -ve

echo Install rpitx - some package need internet connection -

sudo apt-get update
sudo apt-get install -y libsndfile1-dev git imagemagick libfftw3-dev
#For rtl-sdr use
#sudo apt-get install -y rtl-sdr buffer

# We use CSDR as a dsp for analogs modes thanks to HA7ILM
git clone https://github.com/simonyiszk/csdr
patch -i csdrpizero.diff csdr/Makefile
pushd csdr 
make 
sudo make install
popd

pushd src 
git clone https://github.com/F5OEO/librpitx
pushd librpitx/src
make
popd

pushd pift8
git clone https://github.com/kgoba/ft8_lib
popd

make
sudo make install
popd

printf "\n\n"
printf "In order to run properly, rpitx need to modify /boot/config.txt. Are you sure (y/n) "
read -r CONT

if [ "$CONT" = "y" ]; then
  echo "Set GPU to 250Mhz in order to be stable"
   LINE='gpu_freq=250'
   FILE='/boot/config.txt'
   grep -qF "$LINE" "$FILE"  || echo "$LINE" | sudo tee --append "$FILE"
   echo "Installation completed !"
else
  echo "Warning : Rpitx should be instable and stop from transmitting !";
fi


