import json

#
# Get List of Prompts
#
def prompts():
    with open("prompts/prompts.json") as file:
        return json.load(file)

#
# Respond to Prompt
#
def respond(parms):

    # get prompt parameters
    name = parms["name"]
    arguments = parms["arguments"]

    # get matched prompt
    available_prompts = prompts()
    matched_prompt = None
    for prompt in available_prompts:
        if name == prompt["name"]:
            matched_prompt = prompt
            break

    # check matched prompt
    if not matched_prompt:
        raise RuntimeError("Error: unable to locate prompt")

    # read prompt
    with open(f"prompts/{name}.json", "r") as file:
        messages = json.load(file)

    # handle arguments
    for arg,val in arguments.items():
        for message in messages:
            if message["content"]["type"] == "text":
                message["content"]["text"].replace("{{%s}}"%(arg), val)
            elif message["content"]["type"] == "resource":
                message["content"]["resource"]["uri"].replace("{{%s}}"%(arg), val)

    # return response
    return messages
