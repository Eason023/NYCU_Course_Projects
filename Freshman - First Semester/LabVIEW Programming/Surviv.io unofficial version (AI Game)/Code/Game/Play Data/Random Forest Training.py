import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
import matplotlib.pyplot as plt
from joblib import dump, load

# 載入資料
Surviv_train = pd.read_csv("Path\\to\\TrainingData.csv")

# 資料準備
X = Surviv_train.drop(columns=["Action"])  # 特徵
y = Surviv_train["Action"]  # 行為標籤

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# 建立隨機森林模型
rf_model = RandomForestClassifier(n_estimators=100, max_depth=25, random_state=42)

# 訓練模型
rf_model.fit(X_train, y_train)

# 測試模型
y_pred = rf_model.predict(X_test)
print("Accuracy:", accuracy_score(y_test, y_pred))

dump(rf_model, "Path\\to\\Saved_rf_model.joblib")

# 特徵重要性
importances = rf_model.feature_importances_
features = X.columns

# 視覺化
plt.barh(features, importances)
plt.xlabel("Feature Importance")
plt.ylabel("Features")
plt.show()