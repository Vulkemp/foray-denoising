# This python scripts downloads the intel sponza scene and unpacks it.
# Files from https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html
# Working as of 17.12.2022

import os

# download links
downloads = {
    "sponza_base_scene" : "https://cdrdv2.intel.com/v1/dl/getContent/726594",
    "colorful_curtains" : "https://cdrdv2.intel.com/v1/dl/getContent/726650",
    "ivy" : "https://cdrdv2.intel.com/v1/dl/getContent/726656",
    "trees" : "https://cdrdv2.intel.com/v1/dl/getContent/726662",
    "emissive_candles" : "https://cdrdv2.intel.com/v1/dl/getContent/726676",
}

# https://stackoverflow.com/questions/15644964/python-progress-bar-and-downloads
import urllib.request
from tqdm import tqdm

class DownloadProgressBar(tqdm):
    def update_to(self, b=1, bsize=1, tsize=None):
        if tsize is not None:
            self.total = tsize
        self.update(b * bsize - self.n)


def download_url(url, output_path):
    with DownloadProgressBar(unit='B', unit_scale=True,
                             miniters=1, desc=url.split('/')[-1]) as t:
        urllib.request.urlretrieve(url, filename=output_path, reporthook=t.update_to)

if not os.path.exists("downloads/"):
    print("Create download folder")
    os.mkdir("downloads")

print("Downloading zip files. Note that this will take a while as the files are multiple gigabytes large.")
for filename in downloads:
    if not os.path.exists("downloads/"+filename+".zip"):
        print("\t => Downloading "+filename+" ...")
        download_url(downloads[filename], "downloads/"+filename+".zip")
    else:
        print("\t => Skipping "+filename+" ...")

print("Extracting files ...")

# https://stackoverflow.com/questions/3451111/unzipping-files-in-python
import zipfile

for filename in downloads:
    with zipfile.ZipFile("downloads/"+filename+".zip", 'r') as zip_ref:
        print("Extracting "+filename+" ...")
        zip_ref.extractall("./data/intel-sponza/")

print("Clean download directory \"" + os.path.abspath("downloads") + "\"? y/n")
choice = input()

import shutil

if choice == "y":
    shutil.rmtree(os.path.abspath("downloads"))
print("Python script finished")