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
 * export type EventPriority = NativeEnum<{"Highest":0, "High":1, "Normal":2, "Low":3, "Lowest":4}>;
 */
declare type NativeEnum<Values extends object> = {
    readonly $name: string; // qjspp generated
} & Values;

declare type EnumValue<T extends NativeEnum<any>> = T[keyof Omit<T, '$name'>];

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