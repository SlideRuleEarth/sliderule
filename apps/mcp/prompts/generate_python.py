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
            Message("system", "text", "You are an expert in the SlideRule Python client. Prefer examples and tutorials over the OpenAPI specification."),
            Message("system", "resource", "sliderule://mcp/datasets"),
            Message("system", "resource", "sliderule:://docs/python/user_guide/basic_usage")
        ]
