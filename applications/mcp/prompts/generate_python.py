from prompt import UserMessage, UserPrompt

#
# generate_python
#
class GeneratePython(UserPrompt):

    def __init__(self):
        super().__init__(
            name="generate_python",
            description="Generate Python code using the SlideRule client",
            arguments=[self.QUESTION]
        )

    def messages(self):
        return [
            UserMessage("system", "text", "You are an expert in the SlideRule Python client. Prefer examples and tutorials over the OpenAPI specification."),
            UserMessage("system", "resource", "sliderule://mcp/datasets"),
            UserMessage("system", "resource", "sliderule:://docs/python/user_guide/basic_usage")
        ]
