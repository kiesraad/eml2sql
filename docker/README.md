# Running with Docker

This describes how to fill an `eml.sqlite` database, and run the html server `emlserv` using Docker.
With Docker it is not needed to install any software except Docker.
To build the Docker image run the Docker build command in the docker directory:

```sh
docker build -t eml2sql:latest .
```

## Generate the eml.sqlite file

To fill the `eml.sqlite` directory run the following command:

```sh
docker run --rm -v <data directory>:/data eml2sql:latest
```

Here `<data directory>` is the directory containing the eml files.
The tool will recusively search subdirectories for eml files.
In the `<data directory>` the `eml.sqlite` file will be generated.

When the Docker image is build it will compile the source code from GitHub.
To run with sources compiled from a local checked out direcotry add a volume to the local sources `-v <source directory>:/source` to the Docker run command.
This will recompile the `emlconv` from local sources.
The script in the Docker container tests if `emlconv` exitst.
To retrigger the build after a prior run, remove the `emlconv` executable.

```sh
docker run --rm -v <data directory>:/data -v <source directory>:/source eml2sql:latest
```

Here `<source directory` should be the directory containing the `eml2sql` directory.

## Host the `emlserv` webserver with Docker

The code contains a webserver `emlserv` to show the data in the browser.
To host the webserver run Docker with the following command:

```sh
docker run --rm -v <data directory>:/data -p <port>:8081 eml2sql:latest html
```

With the argument `html` the webserver will be activated and will run on the port you specified.
Here `<port>` is the port you want the webserver to be available.
For example when you use `8080` the webserver will be available at `http://localhost:8080`.
This command can also include the source volume to run against your local directory.
To stop the docker container, run `docker ps` to see what id the container has,
and run `docker stop <id>` to stop the container.
