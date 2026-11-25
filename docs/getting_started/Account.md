# Logging In

Use of SlideRule services requires a [GitHub](https://github.com) account to identify you as a user.  If you do not already have one, you can create a free account at [https://github.com/signup](https://github.com/signup).

SlideRule uses GitHub only to verify who you are; it does not need access to your repositories.  When you log in, the SlideRule login service (`login.slideruleearth.io`) verifies your identity with GitHub and issues you a SlideRule access token that is included in your requests to SlideRule services.

## Logging in from the Python client

The first time you initialize the Python client, it will automatically start the GitHub login process:

```python
from sliderule import sliderule
sliderule.init()
```

You will be prompted with something like the following, and a browser window will be opened for you (if one is available):

```
============================================================
GitHub Authentication Required
============================================================

Please visit: https://github.com/login/device

And enter this code: ABCD-1234

============================================================
Waiting for authorization...  /
```

Go to the link shown, enter the code, and authorize the SlideRule application.  The client finishes logging in once you have done so.  You have about one minute to complete this step before the client gives up; if it times out, run `sliderule.init()` again.

Your access token is cached in `~/.cache/sliderule/` so that you do not need to log in every time you run a script.  When the token expires, the client will prompt you to log in again.

## Logging in without user interaction

For scripts that run unattended (e.g. CI/CD pipelines, batch jobs, or remote servers without a browser), you can log in using a GitHub [personal access token (classic)](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens) instead.  Provide the token in one of the following ways:

* Set the `SLIDERULE_GITHUB_TOKEN` environment variable to the token
* Pass the token when initializing the client: `sliderule.init(github_token="<PAT>")`
* Pass the token when creating a session: `sliderule.create_session(cluster="<cluster>", github_token="<PAT>")`

See the [Private Clusters](../user_guide/articles/260120_private_clusters.md) article for the token permissions required.  Treat your token like a password; do not commit it to source control or share it.

## Logging in from the web client

In the [SlideRule Web Client](https://client.slideruleearth.io), click the **Login** button in the top toolbar and follow the prompts to sign in with GitHub.

## What your account gives you access to

Any GitHub user can log in and use the public SlideRule cluster.  Access to additional services, such as deploying private clusters, requires membership in the [SlideRuleEarth](https://github.com/SlideRuleEarth) organization.  See the [Security Model](../user_guide/articles/260312_security_model.md) and [Private Clusters](../user_guide/articles/260120_private_clusters.md) articles for details.
