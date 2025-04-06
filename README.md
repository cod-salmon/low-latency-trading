To run the whole code (5 clients, one exchange) in Docker:
1. Go to `./low-latency-app`.
2. Run `docker compose up -d --force-recreate && docker compose build --no-cache` (this compiles `Dockerfile` and `compose.yaml` and runs only `Dockerfile`)
3. Run `docker compose up && docker compose build` (this actually runs the command inside `compose.yaml`).

Once finished, the `perf_analysis.ipynb` notebook can be run by:
1. Removing `command` line in `compose.yaml`.
2. Commenting out any `RUN make`/`RUN camke` lines in the `Dockerfile`; and uncommenting the code below which installs python, jupyter and all necessary modules.
3. Do again  `docker compose up -d --force-recreate && docker compose build --no-cache` and then `docker compose up && docker compose build`. It will give you a link (http://127.0.0.1:8888/tree) where you can open up and run the jupyter notebook in your browser (with the updated log files you just created). NOTE the CPU_FREQ variable in `perf_analysis.ipynb` (this is something you need to work out).

NOTE: Adding `ninja` to `.gitignore`. I got the `ninja` executable for the 1.10.2 version (https://github.com/ninja-build/ninja/releases). I copied the executable into this folder and let Docker know it should look here for it too (done this by setting `ENV PATH=/usr/src/:$PATH` in the `Dockerfile`).