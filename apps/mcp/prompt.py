# ###############################
# Base Message Class
# ###############################

class Message:

    def __init__(self, role, msg_type, content):
        self.role = role
        self.type = msg_type
        self.content = content

    @property
    def definition(self):
        if self.type == "text":
            return {
                "role": self.role,
                "content": {
                    "type": "text",
                    "text": self.content
                }
            }
        elif self.type == "resource":
            return {
                "role": self.role,
                "content": {
                    "type": "resource",
                    "resource": {
                        "uri": self.content
                    }
                }
            }
        else:
            raise RuntimeError(f"invalid message type: {self.type}")

# ###############################
# Base Prompt Class
# ###############################

class Prompt:

    QUESTION = {
        "name": "question",
        "description": "Scientific question or desired analysis",
        "required": True
    }

    def __init__(self, name, description, arguments):
        self.name = name
        self.description = description
        self.arguments = arguments

    @property
    def definition(self):
        return {
            "name": self.name,
            "description": self.description,
            "arguments": self.arguments
        }

    def messages(self): # pure virtual
        raise NotImplementedError()

    def respond(self, arguments):
        msgs = self.messages()
        if "question" in arguments:
            msgs.append(Message("user", "text", arguments["question"]))
        return [msg.definition for msg in msgs]
