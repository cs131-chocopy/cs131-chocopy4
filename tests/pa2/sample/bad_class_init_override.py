class A(object):
    x:int = 1
    def __init__(self:"A", x:int) -> object: # Bad override
        pass
A(1)

