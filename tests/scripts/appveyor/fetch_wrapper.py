import requests
import zipfile
import os
import shutil

from io import BytesIO

URL = 'http://sonarqube.com/static/cpp/build-wrapper-win-x86.zip'
DIR = 'C:\\ProgramData\\chocolatey\\bin'

request = requests.get(URL)
zip_file = zipfile.ZipFile(BytesIO(request.content))

for member in zip_file.namelist():
    filename = os.path.basename(member)
    # skip directories
    if not filename:
        continue

    # copy file (taken from zipfile's extract)
    source = zip_file.open(member)
    target = open(os.path.join(DIR, filename), "wb")
    with source, target:
        shutil.copyfileobj(source, target)
