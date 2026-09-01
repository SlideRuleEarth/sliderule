from prompt import Message, Prompt

#
# generate_python
#
class GeneratePython(Prompt):

    def __init__(self):
        super().__init__(
            name="generate_python",
            description="Generate Python code using the SlideRule client",
            arguments=[self.QUESTION]
        )

    def messages(self):
        return [
            Message("user", "text", "You are an expert in the SlideRule Python client. Prefer examples and tutorials over the OpenAPI specification."),
            Message("user", "resource", "sliderule://mcp/datasets"),
            Message("user", "resource", "sliderule://docs/python/user-guide/basic-usage")
        ]
