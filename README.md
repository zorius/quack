# Quack (HyprMX fork)

Quack provides Java (Android and desktop) bindings to JavaScript engines. 

The HyprmMX fork fixes a few compilation problems on the original project and is compiled to **support 16KB page sizes**.

1. QuickJS dependency was missing and the project wasn't compilable at all;
2. AGP and Gradle wrapper version were so outdated we couldn't run a build;
3. Kotlin version outdated.

> [!IMPORTANT]
>
> Bumping AGP and gradle versions was important to help supporting 16KB page sizes but also introduced a some other problems. Prefedined NDK version wasn't compatinble anymore with the `minSdkVersion` defined.

## Steps to update the project

### Updating gradle build scrips

1. Open up `quack/quack-android/build.gradle` and check
   - `ndkVersion` ()
   - Make sure `targetSdkVersion` and `compileSdkVersion` are the same
   - Make sure minSdkVersion isn't lower than 17. Lower versions will make native code broken and uncompilable
2. Open up `quack/quack-android/src/main/jni/CMakeLists.txt` and ensure the following line is there, to make the `*.so` files 16KB aligned

```cmake
target_link_options(quack PRIVATE "-Wl,-z,max-page-size=16384")
```



### Assemble the project

To assemble and get new artifacts we can either use Android Studio and `Build -> Make Project` or open up a terminal window and execute
```bash
./gradlew assemble
```

> [!NOTE]
>
> Make sure you're inside projects root directory to execute gradlew commands.

Once the projects get assembled you can find `quack-android-release.aar` artifact inside `quack/quack-android/build/outputs/aar/`.
