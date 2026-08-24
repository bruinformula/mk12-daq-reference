# Mk. 12 DAQ Reference Repo

### Folders:
- `Docs/` -> VectorNav programming library documentation
- `Drivers/` -> Drivers for DAQ sensors
- `Filters/` -> Filter modules for DAQ sensors

### Notes
- VectorNav setup is in `Drivers/vnproglib/README.md`

## Using Submodules

Creating a new repo and adding Mk-12-Reference to it (one-time):
```shell
git submodule add https://github.com/bruinformula/mk12-daq-reference mk12-daq-reference
git commit -m "Added mk12-daq-reference as a submodule"
git push
```

If you're cloning any board projects (which already have mk12-daq-reference) to your computer:
```shell
git clone --recurse-submodules <link to mdu code, sdu code, etc.>

# If you already accidentally cloned without --recurse-submodules, then run this afterward:
# (This will update empty submodules with files)
git submodule update --init --recursive
```

Pulling (when mk12-daq-reference is in the repo):
```shell
# Pull the normal code
git pull

# Also pull submodule code whenever it gets updated (and commit if u need to update)
git submodule update --remote --merge
git add mk12-daq-reference
git commit -m "Updated submodule pointer"
git push
```

Making changes to Mk. 12 DAQ Reference:
```shell
git clone https://github.com/bruinformula/mk12-daq-reference
cd mk12-daq-reference
# Make changes, git add, git commit, git push, standard stuff

cd mk12-mdu-code # Or whichever board
# Then run in the board's folder:
git submodule update --remote --merge

# Then commit your pointer update in the board folder!
git add mk12-daq-reference
git commit -m "Updated submodule pointer"
git push
```