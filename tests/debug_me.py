"""A small script to test pdb debugging via tmux-pilot."""


def fibonacci(n):
    if n <= 1:
        return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b


def main():
    numbers = [5, 10, 15]
    results = {}
    for n in numbers:
        result = fibonacci(n)
        results[n] = result
        print(f"fib({n}) = {result}")

    total = sum(results.values())
    print(f"Total: {total}")
    return results


if __name__ == "__main__":
    main()
