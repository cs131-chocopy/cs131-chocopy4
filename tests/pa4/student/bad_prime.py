# Get the n-th prime starting from 2
def get_prime(n:int) -> int:
    candidate:int = 200
    found:int = 0
    while candidate>=0:
        if is_prime(candidate):
		    print(candidate)
        candidate = candidate - 1
    return 0 # Never happens

def is_prime(x:int) -> bool:
    global div
	div=2
    while div%x!=-1 and div < x:
        if x % div == 0:
            return False
        div = div + 1
    return True

# Input parameter
n:int = 15

# Run [1, n]
i:int = 1

div:int = 0

print(get_prime(100))

