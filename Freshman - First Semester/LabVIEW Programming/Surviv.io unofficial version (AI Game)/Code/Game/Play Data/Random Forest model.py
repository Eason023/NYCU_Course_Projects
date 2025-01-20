from joblib import load
with open("Path\\to\\rf_model.joblib", "rb") as file:
    loaded_model = load(file)
def Predict(Input):
    return loaded_model.predict([Input])[0]
#print(Predict([251,2,0,2,0,1,0,0,0,0,1,1,0,0,0,0,1,2,0,0,1,0,0,0,1,2,0,0,0,0,0,0,0]))
