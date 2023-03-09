# Docker Command Line

2021-04-22

### Logging into DockerHub

You can either use your personal account username/password, or you can create an access token to act as your password.  To create an access token, go to your DockerHub account settings (by selecting the drop-down menu next to your account name).  Then navigate to "Account Settings" > "Security" and click button to create "New Access Token".  The access token is used in place of your password.

Then login to dockerhub the command below, supplying the password when prompted:
```bash
$ docker login -u {username}
```

### Logging into GitHub Docker Container Registry

You must use a personal access token with the appropriate scopes to publish and install packages in GitHub Packages. On the GitHub web page, got to your user settings (by selecting your profile image drop-down menu).  Then nativate to "Settings" > "Developer settings" > "Personal access tokens".

Create a new personal access token with the following permissions:
* repo
* read:packages
* write:packages
* delete:packages

Copy the access token string to a local file (for example, the file used below is ~/.github_token).

Then login to the GitHub container registry with docker:
```bash
$ cat ~/.github_token | docker login docker.pkg.github.com -u {github username} --password-stdin
```

### Tag Docker Image

By convention, the {TAG} for a docker image matches the git branch used to build the image.  To list the available images, use `docker images`.

```bash
$ docker tag {IMAGE ID} {REPOSITORY NAME}:{TAG}
```

### Push Docker Image

```bash
$ docker push {REPOSITORY NAME}:{TAG}
```

### Pull Docker Image

```bash
$ docker pull {REPOSITORY NAME}:{TAG}
```

### Common Docker Commands

* List available docker images: `docker images` or use `docker images -a` to see all images including dangling and intermediate ones.
* List running docker containers: `docker ps`
* SSH into running container: `docker exec -it {container name or container id} /bin/bash`
* Clean up dangling images and stopped containers: `docker system prune`
* Delete specific docker image: `docker rmi {IMAGE_ID}` or `docker rmi {REPOSITORY}:{TAG}`
* Display standard out of running container: `docker logs {CONTAINER ID}`
* Detach from a running container: `<ctrl>-p <ctrl>-q`
* Attach to a running container: `docker attach {CONTAINER ID}`
