/**
 * C++ std::optional
 */
declare type optional<T> = T | null;

/**
 * C++ std::vector
 */
declare type vector<T> = Array<T>;

/**
 * C++ std::unordered_map
 */
declare type unordered_map<K extends string, V> = {
    [key: string]: V;
};

/**
 * C++ std::variant
 * @example type MyType = variant<[number, string]>;
 */
declare type variant<Args extends readonly unknown[]> = Args[number];

/**
 * C++ enum (qjspp)
 * @example
 * export type EventPriority = NativeEnum<["Highest", "High", "Normal", "Low", "Lowest"]>;
 */
declare type NativeEnum<Keys extends readonly string[]> = {
    readonly $name: string; // qjspp generated
} & {
    readonly [K in Keys[number]]: number;
};

/**
 * C++ class
 * @template T
 * - T: Class
 * @example class MyClass implements InstanceClassHelper<MyClass> { ... }
 */
declare interface InstanceClassHelper<T> {
    $equals(other: T): boolean;
}

/**
 * C++ pair
 */
declare type pair<K, V> = [K, V];