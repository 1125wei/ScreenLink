// ScreenLink app 模块
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.screencap.screenlink"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.screencap.screenlink"
        minSdk = 26          // Android 8.0+，覆盖 98% 设备
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.09.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.9.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.4")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
    // 本地 OCR（ML Kit 中文，bundled 不依赖 GMS）
    implementation("com.google.mlkit:text-recognition-chinese:16.0.0")
    // HTTP 客户端（OpenAI 兼容 AI 接口）
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
}
